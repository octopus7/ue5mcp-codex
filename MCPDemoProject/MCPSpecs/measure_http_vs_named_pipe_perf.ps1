param(
	[string]$Endpoint = "http://127.0.0.1:17777/mcp/v1/invoke",
	[string]$Token = "change-me",
	[string]$PipeName = "UEMCPToolsBridge",
	[int]$PipeConnectTimeoutMs = 3000,
	[int]$BaselineRuns = 5,
	[int]$Seed = 0,
	[switch]$SkipApply
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$rand = if ($Seed -ne 0) { [System.Random]::new($Seed) } else { [System.Random]::new() }

function Next-Range {
	param(
		[double]$Min,
		[double]$Max
	)

	return $Min + ($rand.NextDouble() * ($Max - $Min))
}

function New-RandomTint {
	return [ordered]@{
		r = [math]::Round((Next-Range -Min 0.20 -Max 1.00), 3)
		g = [math]::Round((Next-Range -Min 0.20 -Max 1.00), 3)
		b = [math]::Round((Next-Range -Min 0.20 -Max 1.00), 3)
		a = [math]::Round((Next-Range -Min 0.12 -Max 0.35), 3)
	}
}

function Convert-ResponseBodyToObject {
	param(
		[string]$Body
	)

	if ([string]::IsNullOrWhiteSpace($Body)) {
		return [pscustomobject]@{
			status = "TransportError"
			error = "Empty response body."
		}
	}

	try {
		return $Body | ConvertFrom-Json -Depth 80
	}
	catch {
		return [pscustomobject]@{
			status = "TransportError"
			error = "Response is not valid JSON."
			raw = $Body
		}
	}
}

function Get-PropValue {
	param(
		[object]$Object,
		[string]$Name,
		$DefaultValue
	)

	$prop = $Object.PSObject.Properties[$Name]
	if ($prop -and $null -ne $prop.Value) {
		return $prop.Value
	}

	return $DefaultValue
}

function Invoke-NamedPipeRequestBody {
	param(
		[string]$BodyJson
	)

	$client = $null
	$writer = $null
	$reader = $null

	try {
		$client = [System.IO.Pipes.NamedPipeClientStream]::new(
			".",
			$PipeName,
			[System.IO.Pipes.PipeDirection]::InOut,
			[System.IO.Pipes.PipeOptions]::None
		)

		$client.Connect($PipeConnectTimeoutMs)

		$writer = [System.IO.BinaryWriter]::new($client, [System.Text.Encoding]::UTF8, $true)
		$reader = [System.IO.BinaryReader]::new($client, [System.Text.Encoding]::UTF8, $true)

		$payloadBytes = [System.Text.Encoding]::UTF8.GetBytes($BodyJson)
		$writer.Write([int]$payloadBytes.Length)
		$writer.Write($payloadBytes)
		$writer.Flush()

		$responseLength = $reader.ReadInt32()
		if ($responseLength -lt 0 -or $responseLength -gt (8 * 1024 * 1024)) {
			throw "Invalid response length from named pipe: $responseLength"
		}

		$responseBytes = $reader.ReadBytes($responseLength)
		if ($responseBytes.Length -ne $responseLength) {
			throw "Incomplete response read from named pipe."
		}

		return [System.Text.Encoding]::UTF8.GetString($responseBytes)
	}
	finally {
		if ($reader) { $reader.Dispose() }
		if ($writer) { $writer.Dispose() }
		if ($client) { $client.Dispose() }
	}
}

function Invoke-McpRequest {
	param(
		[string]$Transport,
		[string]$RequestId,
		[bool]$DryRun,
		[object]$Payload,
		[string]$Scenario
	)

	$swPrep = [System.Diagnostics.Stopwatch]::StartNew()
	$envelope = [ordered]@{
		request_id = $RequestId
		tool_id = "widget.apply_ops"
		dry_run = $DryRun
		payload = $Payload
	}
	if ($Transport -eq "named_pipe") {
		$envelope["auth_token"] = $Token
	}
	$swPrep.Stop()

	$swJson = [System.Diagnostics.Stopwatch]::StartNew()
	$body = $envelope | ConvertTo-Json -Depth 80 -Compress
	$swJson.Stop()

	$swTransport = [System.Diagnostics.Stopwatch]::StartNew()
	$httpStatus = 0
	$responseRaw = ""
	$transportError = ""

	if ($Transport -eq "http") {
		try {
			$responseObj = Invoke-RestMethod `
				-Uri $Endpoint `
				-Method Post `
				-ContentType "application/json" `
				-Headers @{ Authorization = "Bearer $Token" } `
				-Body $body `
				-TimeoutSec 120
			$httpStatus = 200
			$responseRaw = $responseObj | ConvertTo-Json -Depth 80 -Compress
		}
		catch {
			if ($_.Exception.Response -and $_.Exception.Response.StatusCode) {
				$httpStatus = [int]$_.Exception.Response.StatusCode
			}
			else {
				$httpStatus = -1
			}

			if ($_.ErrorDetails -and $_.ErrorDetails.Message) {
				$responseRaw = $_.ErrorDetails.Message
			}
			else {
				$transportError = $_.Exception.Message
			}
		}
	}
	elseif ($Transport -eq "named_pipe") {
		try {
			$responseRaw = Invoke-NamedPipeRequestBody -BodyJson $body
			$httpStatus = 0
		}
		catch {
			$httpStatus = -1
			$transportError = $_.Exception.Message
		}
	}
	else {
		throw "Unsupported transport: $Transport"
	}

	$swTransport.Stop()

	$swParse = [System.Diagnostics.Stopwatch]::StartNew()
	$response = Convert-ResponseBodyToObject -Body $responseRaw
	$swParse.Stop()

	$mcpStatus = [string](Get-PropValue -Object $response -Name "status" -DefaultValue "Unknown")
	$applied = [int](Get-PropValue -Object $response -Name "applied_count" -DefaultValue 0)
	$skipped = [int](Get-PropValue -Object $response -Name "skipped_count" -DefaultValue 0)
	$warnings = @(Get-PropValue -Object $response -Name "warnings" -DefaultValue @()).Count
	$errors = @(Get-PropValue -Object $response -Name "errors" -DefaultValue @()).Count
	$error = [string](Get-PropValue -Object $response -Name "error" -DefaultValue "")
	if (-not [string]::IsNullOrWhiteSpace($transportError)) {
		$error = $transportError
	}

	$totalMs = $swPrep.Elapsed.TotalMilliseconds + $swJson.Elapsed.TotalMilliseconds + $swTransport.Elapsed.TotalMilliseconds + $swParse.Elapsed.TotalMilliseconds

	return [pscustomobject]@{
		transport = $Transport
		scenario = $Scenario
		request_id = $RequestId
		dry_run = $DryRun
		http_status = $httpStatus
		mcp_status = $mcpStatus
		applied_count = $applied
		skipped_count = $skipped
		warning_count = $warnings
		error_count = $errors
		prep_ms = [math]::Round($swPrep.Elapsed.TotalMilliseconds, 3)
		json_ms = [math]::Round($swJson.Elapsed.TotalMilliseconds, 3)
		transport_ms = [math]::Round($swTransport.Elapsed.TotalMilliseconds, 3)
		parse_ms = [math]::Round($swParse.Elapsed.TotalMilliseconds, 3)
		total_ms = [math]::Round($totalMs, 3)
		error = $error
	}
}

$widgets = @(
	"/Game/UI/Widget/WBP_MCPDummyPopup1",
	"/Game/UI/Widget/WBP_MCPDummyPopup2",
	"/Game/UI/Widget/WBP_MCPDummyPopup3",
	"/Game/UI/Widget/WBP_MCPDummyPopup4",
	"/Game/UI/Widget/WBP_MCPDummyPopup5"
)

$tintByWidget = [ordered]@{}
foreach ($widget in $widgets) {
	$tintByWidget[$widget] = New-RandomTint
}

$rows = @()
$transports = @("http", "named_pipe")
foreach ($transport in $transports) {
	for ($i = 1; $i -le $BaselineRuns; $i++) {
		$baselinePayload = [ordered]@{
			schema_version = "1.0"
			mode = "patch"
			target_widget_path = "/Game/UI/Widget/WBP_MCPDummyPopup1"
			operations = @()
		}
		$rows += Invoke-McpRequest `
			-Transport $transport `
			-RequestId ("$transport-baseline-$i") `
			-DryRun $true `
			-Payload $baselinePayload `
			-Scenario "baseline_noop"
	}

	$id = 1
	foreach ($widget in $widgets) {
		$tint = $tintByWidget[$widget]
		$payload = [ordered]@{
			schema_version = "1.0"
			mode = "patch"
			target_widget_path = $widget
			operations = @(
				[ordered]@{
					op = "update_widget"
					key = "IdentityTintPanel"
					properties = [ordered]@{
						border_draw_as = "rounded_box"
						brush_corner_radius = 12
						brush_tint_r = $tint.r
						brush_tint_g = $tint.g
						brush_tint_b = $tint.b
						brush_tint_a = $tint.a
					}
				}
			)
		}

		$rows += Invoke-McpRequest `
			-Transport $transport `
			-RequestId ("$transport-dry-$id") `
			-DryRun $true `
			-Payload $payload `
			-Scenario "tint_update_dry_run"

		if (-not $SkipApply) {
			$rows += Invoke-McpRequest `
				-Transport $transport `
				-RequestId ("$transport-apply-$id") `
				-DryRun $false `
				-Payload $payload `
				-Scenario "tint_update_apply"
		}

		$id++
	}
}

$summaryRows = @()
$groups = $rows | Group-Object -Property @{ Expression = { "$($_.transport)|$($_.scenario)" } }
foreach ($group in $groups) {
	$keyParts = $group.Name.Split("|")
	$groupRows = $group.Group
	$summaryRows += [pscustomobject]@{
		transport = $keyParts[0]
		scenario = $keyParts[1]
		count = $groupRows.Count
		avg_transport_ms = [math]::Round((($groupRows | Measure-Object -Property transport_ms -Average).Average), 3)
		avg_total_ms = [math]::Round((($groupRows | Measure-Object -Property total_ms -Average).Average), 3)
		min_total_ms = [math]::Round((($groupRows | Measure-Object -Property total_ms -Minimum).Minimum), 3)
		max_total_ms = [math]::Round((($groupRows | Measure-Object -Property total_ms -Maximum).Maximum), 3)
		success_count = @($groupRows | Where-Object { $_.mcp_status -eq "Success" }).Count
		error_count = @($groupRows | Where-Object { $_.error_count -gt 0 -or -not [string]::IsNullOrWhiteSpace($_.error) }).Count
	}
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outDir = Join-Path $PSScriptRoot "perf_results"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$detailPath = Join-Path $outDir ("transport_perf_detail_" + $timestamp + ".csv")
$summaryPath = Join-Path $outDir ("transport_perf_summary_" + $timestamp + ".json")

$rows | Export-Csv -Path $detailPath -NoTypeInformation -Encoding UTF8
[ordered]@{
	endpoint = $Endpoint
	pipe_name = $PipeName
	pipe_connect_timeout_ms = $PipeConnectTimeoutMs
	seed = $Seed
	baseline_runs = $BaselineRuns
	summary = $summaryRows
	detail = $rows
} | ConvertTo-Json -Depth 80 | Set-Content -Path $summaryPath -Encoding UTF8

Write-Host ""
Write-Host "=== Transport Summary ==="
$summaryRows | Sort-Object transport, scenario | Format-Table -AutoSize

Write-Host ""
Write-Host "=== Tint Values ==="
$tintByWidget.GetEnumerator() | ForEach-Object {
	$widget = $_.Key
	$tint = $_.Value
	[pscustomobject]@{
		widget = $widget
		tint_r = $tint.r
		tint_g = $tint.g
		tint_b = $tint.b
		tint_a = $tint.a
	}
} | Format-Table -AutoSize

Write-Host ""
Write-Host ("Detail CSV : {0}" -f $detailPath)
Write-Host ("Summary JSON: {0}" -f $summaryPath)
