[English](./ROADMAP.md) | [한국어](./ROADMAP_KO.md)

# Development Roadmap

## Vision

Build a generic, editor-only Unreal MCP plugin that automates UI asset workflows and scales feature coverage over time.

## Principles

- Editor-only scope (no runtime module).
- Safe default behavior with dry-run and clear change reports.
- Preserve user-edited widget assets by default.
- Incremental delivery with strong test coverage.

## Phases

### Phase 1 - Patch-first MVP

- Implement folder-path-based image import rules (project-level, multiple targets).
- Automate UI texture import settings on import/reimport.
- Create Widget Blueprints from C++ `UUserWidget` classes.
- Apply widget tree and binding changes in `patch` mode only.
- Add repair/reparent operations for MCP-managed nodes only.

### Phase 2 - Stability and Tooling

- Add idempotency checks and conflict reporting.
- Expand validation and dry-run diff output.
- Add regression tests for import, widget generation, binding, and repair paths.
- Improve diagnostics and command-level error codes.

### Phase 3 - Scope Expansion

- Extend UI automation features (style conventions, asset naming, structure utilities).
- Add richer migration/repair workflows for existing projects.
- Improve batch operations and team-scale usage ergonomics.

## Future Work: JSON SoT Mode

`patch` mode is the initial delivery model.

JSON SoT mode is explicitly deferred and tracked in:

- [JSON SoT Future Plan](./JSON_SOT_PLAN.md)
