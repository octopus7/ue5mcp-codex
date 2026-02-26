# MCP Specs

- `widget_patch.sample.json`: example payload for `widget.create_or_patch` and `widget.apply_ops`
- `widget_repair.sample.json`: example payload for `widget.repair_tree`

Widget patch operations support:

- `add_widget` with optional `properties.insert_index` (or top-level `insert_index`)
- `reparent_widget` with optional `properties.insert_index` (or top-level `insert_index`)
- `reorder_widget` with required `properties.insert_index` (or top-level `insert_index` / `new_index`)
- Existing ops: `update_widget`, `remove_widget`, `bind_widget_ref`, `bind_event`

`update_widget` style properties:

- `UTextBlock`: `text_color_(r|g|b|a)`, `text_justification`
- `UBorder`: `border_draw_as`, `brush_tint_(r|g|b|a)`, `brush_corner_radius`, `brush_outline_(r|g|b|a)`, `brush_outline_width`
- `UButton`: `button_draw_as`, `button_corner_radius`, `button_outline_(r|g|b|a)`, `button_outline_width`, `button_normal_(r|g|b|a)`, `button_hovered_(r|g|b|a)`, `button_pressed_(r|g|b|a)`

Request envelope for local bridge:

```json
{
  "request_id": "req-001",
  "tool_id": "widget.create_or_patch",
  "dry_run": false,
  "payload": {}
}
```
