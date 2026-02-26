[English](./JSON_SOT_PLAN.md) | [한국어](./JSON_SOT_PLAN_KO.md)

# JSON SoT Future Plan

## Status

Deferred. Initial versions will use `patch` mode as the default and only required workflow.

## Why Deferred

- `patch` mode better preserves manual edits made in the Widget Editor.
- Early release risk is lower when avoiding full overwrite semantics.
- Team adoption is easier with safer defaults.

## Planned SoT Model (Later)

- JSON declares `mode: "sot"` explicitly.
- SoT execution requires dry-run preview and explicit overwrite confirmation.
- Conflict and drift reporting are mandatory before apply.
- Safe scoping rules will be defined to limit destructive changes.

## Prerequisites Before Implementation

- Mature patch workflow and repair reliability.
- Sufficient regression test coverage for widget tree and binding operations.
- Team approval of overwrite and rollback policy.
