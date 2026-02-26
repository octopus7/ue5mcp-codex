# MCP Specs

- `widget_patch.sample.json`: example payload for `widget.create_or_patch` and `widget.apply_ops`
- `widget_repair.sample.json`: example payload for `widget.repair_tree`

Widget patch operations support:

- `add_widget` with optional `properties.insert_index` (or top-level `insert_index`)
- `reparent_widget` with optional `properties.insert_index` (or top-level `insert_index`)
- `reorder_widget` with required `properties.insert_index` (or top-level `insert_index` / `new_index`)
- Existing ops: `update_widget`, `remove_widget`, `bind_widget_ref`, `bind_event`

Request envelope for local bridge:

```json
{
  "request_id": "req-001",
  "tool_id": "widget.create_or_patch",
  "dry_run": false,
  "payload": {}
}
```
