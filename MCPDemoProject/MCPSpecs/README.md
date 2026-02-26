# MCP Specs

- `widget_patch.sample.json`: example payload for `widget.create_or_patch` and `widget.apply_ops`
- `widget_repair.sample.json`: example payload for `widget.repair_tree`

Request envelope for local bridge:

```json
{
  "request_id": "req-001",
  "tool_id": "widget.create_or_patch",
  "dry_run": false,
  "payload": {}
}
```
