[English](./README.md) | [한국어](./README_KO.md)

# ue5mcp-codex

Editor-only Unreal Engine 5.7 MCP plugin project focused on automating repetitive UI production workflows.

## Documents

- [Roadmap](./ROADMAP.md)
- [JSON SoT Future Plan](./JSON_SOT_PLAN.md)

## MCP M1 (Implemented)

- Editor-only plugin: `MCPDemoProject/Plugins/UEMCPTools`
- Local endpoint: `POST http://127.0.0.1:17777/mcp/v1/invoke`
- Default specs folder: `MCPDemoProject/MCPSpecs`
- Manual checklist: `MCPDemoProject/MCPSpecs/MANUAL_TEST_CHECKLIST.md`

Available tool IDs:

- `widget.create_or_patch`
- `widget.apply_ops`
- `widget.bind_refs_and_events`
- `widget.repair_tree`
- `import.apply_folder_rules`
- `import.scan_and_repair`

## Quick Usage

1. Open `MCPDemoProject` in Unreal Editor (plugin starts with the editor).
2. Check bridge/token settings in `MCPDemoProject/Config/DefaultEditor.ini`.
3. Send MCP requests to `http://127.0.0.1:17777/mcp/v1/invoke`.

PowerShell example (`widget.create_or_patch`):

```powershell
$payload = Get-Content .\MCPDemoProject\MCPSpecs\widget_patch.sample.json -Raw | ConvertFrom-Json

$body = @{
  request_id = "req-001"
  tool_id    = "widget.create_or_patch"
  dry_run    = $false
  payload    = $payload
} | ConvertTo-Json -Depth 20

Invoke-RestMethod `
  -Uri "http://127.0.0.1:17777/mcp/v1/invoke" `
  -Method Post `
  -ContentType "application/json" `
  -Headers @{ Authorization = "Bearer change-me" } `
  -Body $body
```

Use `dry_run = $true` to preview changes without mutating assets.
