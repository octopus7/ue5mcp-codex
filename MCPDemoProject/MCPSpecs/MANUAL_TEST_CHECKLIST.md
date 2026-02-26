# MCP M1 Manual Test Checklist

1. Start Unreal Editor and verify `UEMCPTools` plugin is enabled.
2. Verify bridge starts on `127.0.0.1:17777`.
3. Copy `widget_patch.sample.json` to a transient runtime file (for example `widget_patch.runtime.json`) and send `widget.create_or_patch`.
4. Confirm target WBP is created (if missing) and operations apply.
5. Confirm the runtime patch file is deleted after a successful invoke.
6. Edit WBP manually and verify there is no automatic patch re-run that overwrites the edit.
7. Trigger conflicting operation on unmanaged widget and confirm skip + warning.
8. Run `widget.repair_tree` with `widget_repair.sample.json` and verify managed node reparenting.
9. Import texture under `/Game/UI/Textures` and confirm UI preset is auto-applied.
10. Run `import.scan_and_repair` and confirm existing textures are corrected.
11. Invoke any tool with `dry_run=true` and confirm no asset mutation while diff/report is returned.
12. Use `add_widget`/`reparent_widget` with `insert_index`, and `reorder_widget` to confirm child insertion/reordering works as requested.
13. Validate `BindWidget` mapping by temporarily renaming one bound widget in WBP and confirm binding failure is detected (no runtime fallback path).
14. Confirm Widget Editor preview and PIE runtime visuals are aligned for sidebar/popup after applying patch commands.
15. Confirm style changes are applied via MCP patch commands only (no runtime C++ visual style overrides).
