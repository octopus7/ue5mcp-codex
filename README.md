[English](./README.md) | [한국어](./README_KO.md)

# ue5mcp-codex

Minimal Unreal Engine 5.7 project shell with the base game module, `BasicMap`, and the `OctoMCP` editor bridge plugin.

## Project State

- Startup map: `MCPDemoProject/Content/Maps/BasicMap.umap`
- Editor startup map: `BasicMap`
- Default game mode: `GameModeBase`
- OctoMCP editor bridge: `http://127.0.0.1:47831`

## OctoMCP Hello World

`OctoMCP` is an editor-only plugin that exposes a small localhost HTTP bridge. `MCPDemoProject/Plugins/OctoMCP/Scripts/ue_mcp_server.py` is the stdio MCP server that translates MCP tool calls into HTTP requests to the running Unreal Editor.

### What v1 exposes

- Internal health endpoint: `GET http://127.0.0.1:47831/api/v1/health`
- Internal command endpoint: `POST http://127.0.0.1:47831/api/v1/command`
- MCP tool: `ue_get_version_info`

The tool returns:

- `mcpProtocolVersion`
- `engineVersion`
- `buildVersion`
- `projectName`
- `pluginVersion`
- `editorReachable`

### Usage

1. Launch `MCPDemoProject` in Unreal Editor 5.7.
2. Start the MCP server from the repo root:

```powershell
python MCPDemoProject/Plugins/OctoMCP/Scripts/ue_mcp_server.py
```

3. Point your MCP client at the script. Example config:

```json
{
  "mcpServers": {
    "OctoMCP": {
      "command": "python",
      "args": ["MCPDemoProject/Plugins/OctoMCP/Scripts/ue_mcp_server.py"]
    }
  }
}
```

If you launch it from the Unreal project root instead of the repo root, the equivalent path is `Plugins/OctoMCP/Scripts/ue_mcp_server.py`.

### Manual bridge checks

PowerShell:

```powershell
Invoke-WebRequest -UseBasicParsing http://127.0.0.1:47831/api/v1/health

$body = @{
  command = "get_version_info"
  arguments = @{}
  requestId = "manual-check"
} | ConvertTo-Json -Compress

Invoke-WebRequest `
  -UseBasicParsing `
  -Method Post `
  -Uri http://127.0.0.1:47831/api/v1/command `
  -ContentType "application/json" `
  -Body $body
```

The editor must already be running. The Python MCP server does not auto-launch Unreal Editor in v1.
