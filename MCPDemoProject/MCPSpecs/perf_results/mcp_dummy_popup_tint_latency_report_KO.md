# MCP 더미 팝업 틴트 지연 시간 보고서

## 1. 범위

다음 위젯의 `IdentityTintPanel` 틴트 색상을 변경할 때의 지연 시간을 측정했습니다.

- `/Game/UI/Widget/WBP_MCPDummyPopup1`
- `/Game/UI/Widget/WBP_MCPDummyPopup2`
- `/Game/UI/Widget/WBP_MCPDummyPopup3`
- `/Game/UI/Widget/WBP_MCPDummyPopup4`
- `/Game/UI/Widget/WBP_MCPDummyPopup5`

모든 요청은 `POST http://127.0.0.1:17777/mcp/v1/invoke`, `tool_id=widget.apply_ops`로 호출했습니다.

실행 일시: **2026년 3월 1일** (결과 파일 타임스탬프 기준 로컬 시간)

## 2. 환경

| 항목 | 값 |
| --- | --- |
| 프로젝트 | `MCPDemoProject` |
| 에디터 프로세스 | `UnrealEditor` (실행 중) |
| 브리지 엔드포인트 | `127.0.0.1:17777` |
| 인증 | `Bearer change-me` |
| 메인 스크립트 | `MCPSpecs/measure_dummy_popup_tint_perf.ps1` |
| 메인 결과 파일 | `MCPSpecs/perf_results/dummy_tint_summary_20260301_014046.json` |

## 3. 테스트 구성

| 테스트 ID | 설명 | 요청 수 |
| --- | --- | ---: |
| T1 | 기준선(`widget.apply_ops` + 빈 `operations`) | 5 |
| T2 | 팝업 1..5 랜덤 틴트 변경, `dry_run=true` | 5 |
| T3 | 동일 랜덤 틴트 변경, `dry_run=false` | 5 |
| T4 | no-op vs 단일 `update_widget`(popup1) 순차 호출 | 10 + 10 |
| T5 | no-op 5개 병렬 호출 | 5 |

## 4. T1/T2/T3 요약

| 지표 | 값 |
| --- | ---: |
| 기준선 평균 HTTP (T1) | 345.930 ms |
| 기준선 평균 Total (T1) | 356.999 ms |
| Dry-run 평균 HTTP (T2) | 330.037 ms |
| Apply 평균 HTTP (T3) | 331.945 ms |
| Apply - Dry-run 평균 차이 | 1.908 ms |

### Phase 요약

| Phase | Count | 평균 HTTP (ms) | 평균 Total (ms) |
| --- | ---: | ---: | ---: |
| `dry_run` | 5 | 330.037 | 330.317 |
| `apply` | 5 | 331.945 | 332.236 |

### 위젯별 결과 (HTTP 시간)

| 위젯 | Tint (R,G,B,A) | Dry-run (ms) | Apply (ms) | 차이 (Apply-Dry) |
| --- | --- | ---: | ---: | ---: |
| `WBP_MCPDummyPopup1` | (0.931, 0.961, 0.507, 0.122) | 321.187 | 327.113 | +5.926 |
| `WBP_MCPDummyPopup2` | (0.922, 0.482, 0.930, 0.348) | 332.609 | 332.610 | +0.001 |
| `WBP_MCPDummyPopup3` | (0.650, 0.656, 0.557, 0.308) | 332.861 | 332.454 | -0.407 |
| `WBP_MCPDummyPopup4` | (0.998, 0.334, 0.591, 0.169) | 331.769 | 334.150 | +2.381 |
| `WBP_MCPDummyPopup5` | (0.648, 0.584, 0.673, 0.241) | 331.759 | 333.397 | +1.638 |

## 5. T4 no-op vs update (Popup1, 각 10회)

| 케이스 | 평균 (ms) | 최소 (ms) | 최대 (ms) | 적용 건수 |
| --- | ---: | ---: | ---: | ---: |
| no-op (`operations=[]`) | 367.523 | 305.187 | 705.433 | 0 |
| 단일 `update_widget` 틴트 변경 | 332.228 | 325.103 | 333.905 | 1 |

해석: 워밍업 이후에는 두 케이스 모두 약 332~334 ms로 수렴하며, 실제 업데이트 작업 자체의 추가 비용은 작습니다.

## 6. T5 병렬 요청 (no-op 5개)

| 지표 | 값 |
| --- | ---: |
| 총 소요 시간(클라이언트) | 1895.231 ms |
| 요청별 평균 | 1026.049 ms |
| 요청별 최소 | 676.525 ms |
| 요청별 최대 | 1534.556 ms |

요청 완료 시간이 약 +280~300 ms 간격으로 증가하는 패턴을 보여, 실질적으로 병렬 처리보다 직렬/큐 처리에 가깝습니다.

## 7. 구간별 시간 분해 (T2/T3 기준)

| 구간 | Dry-run 평균 (ms) | Apply 평균 (ms) |
| --- | ---: | ---: |
| `prep_ms` | 0.005 | 0.005 |
| `json_ms` | 0.124 | 0.100 |
| `http_ms` | 330.037 | 331.945 |
| `parse_ms` | 0.165 | 0.164 |
| `total_ms` | 330.317 | 332.236 |

추정: 지연의 대부분은 HTTP 구간의 대기/처리 시간이며, payload 준비/직렬화/파싱 비용은 무시 가능한 수준입니다.

## 8. 결론

| 질문 | 결론 |
| --- | --- |
| 이번 틴트 변경에서 MCP 패치 로직이 주 병목인가? | **아님** |
| 지연의 주 원인은 무엇인가? | no-op에서도 나타나는 요청당 고정 대기(약 330ms) |
| 실제 변경 작업이 시간을 크게 늘리는가? | **거의 아님** (dry-run 대비 평균 `~1.9ms`) |
| 병렬 요청이 효율적인가? | **아님** (직렬/큐 처리 패턴) |

## 9. 권장 추가 점검

| 우선순위 | 액션 | 목적 |
| --- | --- | --- |
| 1 | `HandleInvoke` 시작/종료, `Subsystem->InvokeTool` 전후 내부 타이밍 로그 추가 | 전송 대기 vs 툴 실행 시간을 분리 |
| 2 | 에디터 포그라운드 + UI 부하 낮은 상태에서 재측정 | 에디터 틱 스케줄링 영향 검증 |
| 3 | 여러 위젯 변경을 한 요청으로 배치 | 요청당 고정 지연 비용 절감 |
| 4 | HTTP 브리지와 에디터 내부 직접 `InvokeTool` 벤치 비교 | 브리지 오버헤드 정량화 |
