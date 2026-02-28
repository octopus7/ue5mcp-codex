param(
    [string]$Endpoint = "http://127.0.0.1:17777/mcp/v1/invoke",
    [string]$Token = "change-me",
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

function Invoke-McpRequest {
    param(
        [string]$RequestId,
        [string]$ToolId,
        [bool]$DryRun,
        [object]$Payload
    )

    $swPrep = [System.Diagnostics.Stopwatch]::StartNew()
    $envelope = [ordered]@{
        request_id = $RequestId
        tool_id = $ToolId
        dry_run = $DryRun
        payload = $Payload
    }
    $swPrep.Stop()

    $swJson = [System.Diagnostics.Stopwatch]::StartNew()
    $body = $envelope | ConvertTo-Json -Depth 60 -Compress
    $swJson.Stop()

    $headers = @{ Authorization = "Bearer $Token" }
    $swHttp = [System.Diagnostics.Stopwatch]::StartNew()
    $httpStatus = 200
    $responseObj = $null
    $errorText = $null

    try {
        $responseObj = Invoke-RestMethod `
            -Uri $Endpoint `
            -Method Post `
            -ContentType "application/json" `
            -Headers $headers `
            -Body $body `
            -TimeoutSec 120
    }
    catch {
        if ($_.Exception.Response -and $_.Exception.Response.StatusCode) {
            $httpStatus = [int]$_.Exception.Response.StatusCode
        }
        else {
            $httpStatus = -1
        }

        $raw = $null
        if ($_.ErrorDetails -and $_.ErrorDetails.Message) {
            $raw = $_.ErrorDetails.Message
        }
        else {
            try {
                $stream = $_.Exception.Response.GetResponseStream()
                if ($stream) {
                    $reader = New-Object System.IO.StreamReader($stream)
                    $raw = $reader.ReadToEnd()
                    $reader.Dispose()
                }
            }
            catch {
                $raw = $null
            }
        }

        if ($raw) {
            try {
                $responseObj = $raw | ConvertFrom-Json -Depth 60
            }
            catch {
                $responseObj = [pscustomobject]@{
                    status = "HTTPError"
                    error = $raw
                }
            }
        }
        else {
            $errorText = $_.Exception.Message
            $responseObj = [pscustomobject]@{
                status = "HTTPError"
                error = $errorText
            }
        }
    }
    $swHttp.Stop()

    $swParse = [System.Diagnostics.Stopwatch]::StartNew()
    $null = $responseObj | ConvertTo-Json -Depth 60 -Compress
    $swParse.Stop()

    $applied = 0
    $appliedProp = $responseObj.PSObject.Properties["applied_count"]
    if ($appliedProp -and $null -ne $appliedProp.Value) {
        $applied = [int]$appliedProp.Value
    }

    $skipped = 0
    $skippedProp = $responseObj.PSObject.Properties["skipped_count"]
    if ($skippedProp -and $null -ne $skippedProp.Value) {
        $skipped = [int]$skippedProp.Value
    }

    $warningCount = 0
    $warningsProp = $responseObj.PSObject.Properties["warnings"]
    if ($warningsProp -and $null -ne $warningsProp.Value) {
        $warningCount = @($warningsProp.Value).Count
    }

    $errorCount = 0
    $errorsProp = $responseObj.PSObject.Properties["errors"]
    if ($errorsProp -and $null -ne $errorsProp.Value) {
        $errorCount = @($errorsProp.Value).Count
    }

    $statusValue = ""
    $statusProp = $responseObj.PSObject.Properties["status"]
    if ($statusProp -and $null -ne $statusProp.Value) {
        $statusValue = [string]$statusProp.Value
    }

    $errorValue = ""
    $errorProp = $responseObj.PSObject.Properties["error"]
    if ($errorProp -and $null -ne $errorProp.Value) {
        $errorValue = [string]$errorProp.Value
    }

    $total = $swPrep.Elapsed + $swJson.Elapsed + $swHttp.Elapsed + $swParse.Elapsed

    return [pscustomobject]@{
        request_id = $RequestId
        tool_id = $ToolId
        dry_run = $DryRun
        http_status = $httpStatus
        mcp_status = $statusValue
        applied_count = $applied
        skipped_count = $skipped
        warning_count = $warningCount
        error_count = $errorCount
        prep_ms = [math]::Round($swPrep.Elapsed.TotalMilliseconds, 3)
        json_ms = [math]::Round($swJson.Elapsed.TotalMilliseconds, 3)
        http_ms = [math]::Round($swHttp.Elapsed.TotalMilliseconds, 3)
        parse_ms = [math]::Round($swParse.Elapsed.TotalMilliseconds, 3)
        total_ms = [math]::Round($total.TotalMilliseconds, 3)
        error = if ($errorValue) { $errorValue } elseif ($errorText) { $errorText } else { "" }
        response = $responseObj
    }
}

$baselinePayload = [ordered]@{
    schema_version = "1.0"
    mode = "patch"
    target_widget_path = "/Game/UI/Widget/WBP_MCPDummyPopup1"
    operations = @()
}

$baselineRows = @()
for ($i = 1; $i -le $BaselineRuns; $i++) {
    $baselineRows += Invoke-McpRequest `
        -RequestId ("baseline-" + $i) `
        -ToolId "widget.apply_ops" `
        -DryRun $true `
        -Payload $baselinePayload
}

$widgets = @(
    "/Game/UI/Widget/WBP_MCPDummyPopup1",
    "/Game/UI/Widget/WBP_MCPDummyPopup2",
    "/Game/UI/Widget/WBP_MCPDummyPopup3",
    "/Game/UI/Widget/WBP_MCPDummyPopup4",
    "/Game/UI/Widget/WBP_MCPDummyPopup5"
)

$rows = @()
$id = 1
foreach ($widgetPath in $widgets) {
    $tint = New-RandomTint

    $payload = [ordered]@{
        schema_version = "1.0"
        mode = "patch"
        target_widget_path = $widgetPath
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

    $dry = Invoke-McpRequest `
        -RequestId ("dry-" + $id) `
        -ToolId "widget.apply_ops" `
        -DryRun $true `
        -Payload $payload

    $rows += [pscustomobject]@{
        widget = $widgetPath
        phase = "dry_run"
        tint_r = $tint.r
        tint_g = $tint.g
        tint_b = $tint.b
        tint_a = $tint.a
        request_id = $dry.request_id
        tool_id = $dry.tool_id
        dry_run = $dry.dry_run
        http_status = $dry.http_status
        mcp_status = $dry.mcp_status
        applied_count = $dry.applied_count
        skipped_count = $dry.skipped_count
        warning_count = $dry.warning_count
        error_count = $dry.error_count
        prep_ms = $dry.prep_ms
        json_ms = $dry.json_ms
        http_ms = $dry.http_ms
        parse_ms = $dry.parse_ms
        total_ms = $dry.total_ms
        error = $dry.error
    }

    if (-not $SkipApply) {
        $apply = Invoke-McpRequest `
            -RequestId ("apply-" + $id) `
            -ToolId "widget.apply_ops" `
            -DryRun $false `
            -Payload $payload

        $rows += [pscustomobject]@{
            widget = $widgetPath
            phase = "apply"
            tint_r = $tint.r
            tint_g = $tint.g
            tint_b = $tint.b
            tint_a = $tint.a
            request_id = $apply.request_id
            tool_id = $apply.tool_id
            dry_run = $apply.dry_run
            http_status = $apply.http_status
            mcp_status = $apply.mcp_status
            applied_count = $apply.applied_count
            skipped_count = $apply.skipped_count
            warning_count = $apply.warning_count
            error_count = $apply.error_count
            prep_ms = $apply.prep_ms
            json_ms = $apply.json_ms
            http_ms = $apply.http_ms
            parse_ms = $apply.parse_ms
            total_ms = $apply.total_ms
            error = $apply.error
        }
    }

    $id++
}

$baselineHttpAvg = [math]::Round((($baselineRows | Measure-Object -Property http_ms -Average).Average), 3)
$baselineTotalAvg = [math]::Round((($baselineRows | Measure-Object -Property total_ms -Average).Average), 3)

$phaseSummary = $rows |
    Group-Object phase |
    ForEach-Object {
        $avgHttp = [math]::Round((($_.Group | Measure-Object -Property http_ms -Average).Average), 3)
        $avgTotal = [math]::Round((($_.Group | Measure-Object -Property total_ms -Average).Average), 3)
        [pscustomobject]@{
            phase = $_.Name
            count = $_.Count
            avg_http_ms = $avgHttp
            avg_total_ms = $avgTotal
            est_engine_http_ms = [math]::Round(($avgHttp - $baselineHttpAvg), 3)
        }
    }

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outDir = Join-Path $PSScriptRoot "perf_results"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$detailPath = Join-Path $outDir ("dummy_tint_detail_" + $timestamp + ".csv")
$summaryPath = Join-Path $outDir ("dummy_tint_summary_" + $timestamp + ".json")

$rows | Export-Csv -Path $detailPath -NoTypeInformation -Encoding UTF8

$summary = [ordered]@{
    endpoint = $Endpoint
    seed = $Seed
    baseline_runs = $BaselineRuns
    baseline_avg_http_ms = $baselineHttpAvg
    baseline_avg_total_ms = $baselineTotalAvg
    baseline_rows = $baselineRows
    phase_summary = $phaseSummary
    rows = $rows
}
$summary | ConvertTo-Json -Depth 60 | Set-Content -Path $summaryPath -Encoding UTF8

Write-Host ""
Write-Host "=== Baseline (widget.apply_ops + empty operations) ==="
$baselineRows | Select-Object request_id, http_status, mcp_status, http_ms, total_ms | Format-Table -AutoSize
Write-Host ("Baseline Avg -> HTTP: {0} ms / Total: {1} ms" -f $baselineHttpAvg, $baselineTotalAvg)

Write-Host ""
Write-Host "=== Per Request ==="
$rows | Select-Object widget, phase, tint_r, tint_g, tint_b, tint_a, http_status, mcp_status, http_ms, total_ms, applied_count, skipped_count | Format-Table -AutoSize

Write-Host ""
Write-Host "=== Phase Summary ==="
$phaseSummary | Format-Table -AutoSize

Write-Host ""
Write-Host ("Detail CSV : {0}" -f $detailPath)
Write-Host ("Summary JSON: {0}" -f $summaryPath)
