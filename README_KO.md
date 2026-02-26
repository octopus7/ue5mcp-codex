[English](./README.md) | [한국어](./README_KO.md)

# ue5mcp-codex

반복적인 UI 제작 워크플로 자동화에 초점을 둔 에디터 전용 Unreal Engine 5.7 MCP 플러그인 프로젝트입니다.

## 문서

- [로드맵](./ROADMAP_KO.md)
- [JSON SoT 향후 계획](./JSON_SOT_PLAN_KO.md)

## MCP M1 (구현 완료)

- 에디터 전용 플러그인: `MCPDemoProject/Plugins/UEMCPTools`
- 로컬 엔드포인트: `POST http://127.0.0.1:17777/mcp/v1/invoke`
- 기본 스펙 폴더: `MCPDemoProject/MCPSpecs`
- 수동 체크리스트: `MCPDemoProject/MCPSpecs/MANUAL_TEST_CHECKLIST.md`

위젯/UI 구현 정책:

- 위젯 참조는 `BindWidget` 우선이며 런타임 이름 검색 fallback을 두지 않습니다.
- 바인딩 대상 위젯 이름은 관리 명명 규칙 `MCP_<Key>` 와 정확히 일치해야 합니다.
- 시각 스타일은 MCPSpec 패치 + WBP 자산이 SoT입니다.
- 런타임 C++는 동작 로직(이벤트, 입력 모드, 동적 텍스트)만 담당합니다.
