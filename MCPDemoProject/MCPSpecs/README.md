# MCP Specs

- `widget_patch.sample.json`: example payload for `widget.create_or_patch` and `widget.apply_ops` (copy to a transient runtime file before invoke)
- `widget_patch.scroll_grid_popup.sample.json`: sample payload for `WBP_MCPScrollGridPopup`
- `widget_patch.scroll_grid_item.sample.json`: sample payload for `WBP_MCPScrollGridItem`
- `widget_patch.scroll_tile_popup.sample.json`: sample payload for `WBP_MCPScrollTilePopup`
- `widget_patch.scroll_tile_item.sample.json`: sample payload for `WBP_MCPScrollTileItem`
- `widget_patch.sidebar_dummy_section.sample.json`: sample payload for adding `Dummy 1..5` section to `WBP_MCPSidebar`
- `widget_patch.dummy_popup1.sample.json`: sample payload for `WBP_MCPDummyPopup1`
- `widget_patch.dummy_popup2.sample.json`: sample payload for `WBP_MCPDummyPopup2`
- `widget_patch.dummy_popup3.sample.json`: sample payload for `WBP_MCPDummyPopup3`
- `widget_patch.dummy_popup4.sample.json`: sample payload for `WBP_MCPDummyPopup4`
- `widget_patch.dummy_popup5.sample.json`: sample payload for `WBP_MCPDummyPopup5`
- `widget_repair.sample.json`: example payload for `widget.repair_tree`
- `material_create_ui_gradient.sample.json`: sample payload for `material.create_ui_gradient`
- `material_instance_create.sample.json`: sample payload for `material_instance.create`
- `material_instance_set_params.sample.json`: sample payload for `material_instance.set_params`

Widget patch operations support:

- `add_widget` with optional `properties.insert_index` (or top-level `insert_index`)
- `reparent_widget` with optional `properties.insert_index` (or top-level `insert_index`)
- `reorder_widget` with required `properties.insert_index` (or top-level `insert_index` / `new_index`)
- Existing ops: `update_widget`, `remove_widget`, `bind_widget_ref`, `bind_event`
- Top-level `save_on_success` (`bool`, default `false`) saves the target WBP package after a successful non-dry-run invoke

Binding policy:

- C++ widget fields use `BindWidget` and must match generated widget names exactly (managed naming: `MCP_<Key>`).
- `bind_widget_ref` sets `bIsVariable=true` on the target widget for Blueprint variable exposure.
- `bind_widget_ref.variable_name` is legacy/deprecated in M1 and ignored by design (managed identity is preserved).
- Runtime fallback lookup by widget name is not part of the supported binding path.

`update_widget` style properties:

- `UTextBlock`: `text_color_(r|g|b|a)`, `text_justification`
- `UBorder`: `border_draw_as`, `brush_resource_path` (`brush_image_path` alias), `brush_tint_(r|g|b|a)`, `brush_corner_radius`, `brush_outline_(r|g|b|a)`, `brush_outline_width`
- `UButton`: `button_draw_as`, `button_corner_radius`, `button_outline_(r|g|b|a)`, `button_outline_width`, `button_normal_(r|g|b|a)`, `button_hovered_(r|g|b|a)`, `button_pressed_(r|g|b|a)`
- `USlider`: `slider_min_value`, `slider_max_value`, `slider_value`, `slider_step_size`
- Slot layout (`slot_*`) supports: `UVerticalBoxSlot`, `UHorizontalBoxSlot`, `UBorderSlot`, `UCanvasPanelSlot`

Material tools:

- `material.create_ui_gradient`: create/rebuild a UI-domain vertical gradient master material (`TopColor`, `BottomColor`, `TopAlpha`, `BottomAlpha`)
- `material_instance.create`: create a material instance (or update parent if it already exists)
- `material_instance.set_params`: apply scalar/vector parameter overrides to a material instance

Style source of truth policy:

- Widget visual style (color/brush/alignment/padding) is managed in WBP assets.
- MCP patch payloads are transient invoke inputs and should be deleted after successful apply.
- Runtime C++ should only handle behavior (input, events, text values), not persistent visual styling.

Request envelope for local bridge:

```json
{
  "request_id": "req-001",
  "tool_id": "widget.create_or_patch",
  "dry_run": false,
  "payload": {}
}
```

`widget.repair_tree` payload also supports top-level `save_on_success` with the same behavior.
