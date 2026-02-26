# MCP M1 Manual Test Checklist

1. Start Unreal Editor and verify `UEMCPTools` plugin is enabled.
2. Verify bridge starts on `127.0.0.1:17777`.
3. Send `widget.create_or_patch` request with `widget_patch.sample.json` payload.
4. Confirm target WBP is created (if missing) and operations apply.
5. Edit WBP manually, then re-run patch and confirm unmanaged nodes are preserved.
6. Trigger conflicting operation on unmanaged widget and confirm skip + warning.
7. Run `widget.repair_tree` with `widget_repair.sample.json` and verify managed node reparenting.
8. Import texture under `/Game/UI/Textures` and confirm UI preset is auto-applied.
9. Run `import.scan_and_repair` and confirm existing textures are corrected.
10. Invoke any tool with `dry_run=true` and confirm no asset mutation while diff/report is returned.
