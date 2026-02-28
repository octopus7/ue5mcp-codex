# MCP Dummy Popup Tint Latency Report

## 1. Scope

This report measures latency while changing `IdentityTintPanel` tint colors for:

- `/Game/UI/Widget/WBP_MCPDummyPopup1`
- `/Game/UI/Widget/WBP_MCPDummyPopup2`
- `/Game/UI/Widget/WBP_MCPDummyPopup3`
- `/Game/UI/Widget/WBP_MCPDummyPopup4`
- `/Game/UI/Widget/WBP_MCPDummyPopup5`

All calls were sent to `POST http://127.0.0.1:17777/mcp/v1/invoke` with `tool_id=widget.apply_ops`.

Run date: **March 1, 2026** (local machine time from result timestamps).

## 2. Environment

| Item | Value |
| --- | --- |
| Project | `MCPDemoProject` |
| Editor process | `UnrealEditor` (running) |
| Bridge endpoint | `127.0.0.1:17777` |
| Auth | `Bearer change-me` |
| Main script | `MCPSpecs/measure_dummy_popup_tint_perf.ps1` |
| Main result file | `MCPSpecs/perf_results/dummy_tint_summary_20260301_014046.json` |

## 3. Test Set

| Test ID | Description | Request Count |
| --- | --- | ---: |
| T1 | Baseline (`widget.apply_ops` + empty `operations`) | 5 |
| T2 | Random tint update for popup 1..5, `dry_run=true` | 5 |
| T3 | Same random tint update, `dry_run=false` | 5 |
| T4 | No-op vs single `update_widget` (popup1), sequential | 10 + 10 |
| T5 | 5 parallel no-op requests | 5 |

## 4. T1/T2/T3 Summary

| Metric | Value |
| --- | ---: |
| Baseline avg HTTP (T1) | 345.930 ms |
| Baseline avg Total (T1) | 356.999 ms |
| Dry-run avg HTTP (T2) | 330.037 ms |
| Apply avg HTTP (T3) | 331.945 ms |
| Apply - Dry-run avg delta | 1.908 ms |

### Phase Summary

| Phase | Count | Avg HTTP (ms) | Avg Total (ms) |
| --- | ---: | ---: | ---: |
| `dry_run` | 5 | 330.037 | 330.317 |
| `apply` | 5 | 331.945 | 332.236 |

### Per-Widget Results (HTTP time)

| Widget | Tint (R,G,B,A) | Dry-run (ms) | Apply (ms) | Delta (Apply-Dry) |
| --- | --- | ---: | ---: | ---: |
| `WBP_MCPDummyPopup1` | (0.931, 0.961, 0.507, 0.122) | 321.187 | 327.113 | +5.926 |
| `WBP_MCPDummyPopup2` | (0.922, 0.482, 0.930, 0.348) | 332.609 | 332.610 | +0.001 |
| `WBP_MCPDummyPopup3` | (0.650, 0.656, 0.557, 0.308) | 332.861 | 332.454 | -0.407 |
| `WBP_MCPDummyPopup4` | (0.998, 0.334, 0.591, 0.169) | 331.769 | 334.150 | +2.381 |
| `WBP_MCPDummyPopup5` | (0.648, 0.584, 0.673, 0.241) | 331.759 | 333.397 | +1.638 |

## 5. T4 No-op vs Update (Popup1, 10 each)

| Case | Avg (ms) | Min (ms) | Max (ms) | Applied Count |
| --- | ---: | ---: | ---: | ---: |
| No-op (`operations=[]`) | 367.523 | 305.187 | 705.433 | 0 |
| Single `update_widget` tint | 332.228 | 325.103 | 333.905 | 1 |

Interpretation: after warm-up, both cases converge to ~332-334 ms. Real update work adds little compared to fixed request latency.

## 6. T5 Parallel Requests (5 no-op calls)

| Metric | Value |
| --- | ---: |
| Total elapsed (client side) | 1895.231 ms |
| Per-request avg | 1026.049 ms |
| Per-request min | 676.525 ms |
| Per-request max | 1534.556 ms |

Per-request completion times increased in near-serial steps (roughly +280~300 ms per request), indicating queueing/serialized handling rather than true parallel processing.

## 7. Timing Breakdown (from T2/T3 runs)

| Segment | Dry-run Avg (ms) | Apply Avg (ms) |
| --- | ---: | ---: |
| `prep_ms` | 0.005 | 0.005 |
| `json_ms` | 0.124 | 0.100 |
| `http_ms` | 330.037 | 331.945 |
| `parse_ms` | 0.165 | 0.164 |
| `total_ms` | 330.317 | 332.236 |

Inference: latency is dominated by request/handling wait in the HTTP segment; payload prep/serialization/parsing costs are negligible.

## 8. Conclusion

| Question | Answer |
| --- | --- |
| Is MCP patch logic the main bottleneck for this tint update? | **No**, not in this test. |
| What dominates latency? | A near-fixed per-request wait (~330 ms), visible even for no-op calls. |
| Does actual update work materially increase time? | **Very little** (`~1.9 ms` avg vs dry-run). |
| Are parallel requests efficient? | **No**. Results show serialized/queued behavior. |

## 9. Recommended Next Checks

| Priority | Action | Goal |
| --- | --- | --- |
| 1 | Add internal bridge timing logs around `HandleInvoke` start/end and around `Subsystem->InvokeTool` | Separate transport wait vs tool execution time |
| 2 | Re-run while editor is foreground and under low UI load | Verify editor tick scheduling impact |
| 3 | Batch multiple widget ops in one request | Reduce fixed per-request latency cost |
| 4 | Compare HTTP bridge vs in-editor direct `InvokeTool` benchmark | Quantify bridge overhead directly |
