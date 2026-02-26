# Agent Working Rules (Project-Specific)

## 1) Do not change code for simple/static UI edits
- For simple or static content changes (button labels, fixed text, captions, layout constants, style values), do not edit C++ or runtime logic.
- This rule also includes MCP-supported layout edits such as padding, anchor, alignment, position, size, slot size rule/value, and z-order.
- Default path: apply the change through MCP operations and widget/assets.

## 2) MCP/asset-first workflow
- Prefer `widget.apply_ops` / `widget.create_or_patch` (or direct asset edits) for UI content/style updates.
- Treat code changes as a last resort for these cases.

## 3) Code change requires explicit user intent
- Only modify source code for these UI text/style requests when the user explicitly asks for code changes.
- If MCP/asset path is blocked, ask before changing code.

## 4) If violated, fix immediately
- If code was edited for a simple/static UI change by mistake, revert that code change first, then re-apply via MCP/asset path.
