param(
	[string]$Endpoint = "http://127.0.0.1:17777/mcp/v1/invoke",
	[string]$Token = "change-me",
	[string]$WidgetFolder = "/Game/UI/Widget",
	[string]$BaseName = "MCPDummyPopup",
	[string]$ParentClassPath = "/Script/MCPDemoProject.MCPDummyPopupWidget"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$safeBaseName = $BaseName.Trim()
if ([string]::IsNullOrWhiteSpace($safeBaseName)) {
	throw "BaseName cannot be empty."
}

if (-not $safeBaseName.StartsWith("WBP_")) {
	$safeBaseName = "WBP_$safeBaseName"
}

# Always generate a fresh asset name to avoid cache/name collision issues.
$suffix = (Get-Date).ToString("yyyyMMdd_HHmmss_fff")
$targetWidgetPath = "$WidgetFolder/$safeBaseName`_$suffix"

$payload = [ordered]@{
	schema_version = "1.0"
	mode = "patch"
	target_widget_path = $targetWidgetPath
	parent_class_path = $ParentClassPath
	operations = @()
}

$body = [ordered]@{
	request_id = "create-fresh-$suffix"
	tool_id = "widget.create_or_patch"
	dry_run = $false
	payload = $payload
} | ConvertTo-Json -Depth 30

$response = Invoke-RestMethod `
	-Uri $Endpoint `
	-Method Post `
	-ContentType "application/json" `
	-Headers @{ Authorization = "Bearer $Token" } `
	-Body $body

Write-Host ("Created Widget Path: {0}" -f $targetWidgetPath)
Write-Host ""
Write-Host "Response:"
$response | ConvertTo-Json -Depth 30
