# UE5 UMG 커맨드라인 워크플로우 플레이북

이 문서는 Unreal Engine 5 환경(Windows + PowerShell)에서 UMG 작업 시 자주 쓰는 커맨드라인 사용 예시를 정리한 실전 가이드입니다.

## 1. 사전 조건

- Unreal Engine 설치 완료 (예: `E:\WorkTemp\Epic Games\UE_5.7`)
- 프로젝트 파일 존재 (예: `D:\github\ue5mcp-codex\MCPDemoProject\MCPDemoProject.uproject`)
- PowerShell 터미널 사용

## 2. 권장 변수 설정

```powershell
$ProjectRoot = "D:\github\ue5mcp-codex\MCPDemoProject"
$ProjectFile = "$ProjectRoot\MCPDemoProject.uproject"
$UE = "E:\WorkTemp\Epic Games\UE_5.7"
```

## 3. 핵심 커맨드

### 3.1 C++ 에디터 타겟 빌드

```powershell
& "$UE\Engine\Build\BatchFiles\Build.bat" `
  MCPDemoProjectEditor Win64 Development `
  "$ProjectFile" -WaitMutex -FromMsBuild
```

UMG 위젯/서브시스템에 연결된 C++를 수정했을 때 사용합니다.

### 3.2 전체 블루프린트 컴파일(헤드리스)

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$ProjectFile" `
  -run=CompileAllBlueprints -unattended -nop4 -nosplash -NoLogTimes
```

UMG 변경 후 블루프린트 컴파일 상태를 빠르게 검증할 때 사용합니다.

### 3.3 Python 스크립트 커맨드릿 실행

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$ProjectFile" `
  -run=pythonscript -script=D:\path\to\script.py `
  -NoSplash -NullRHI -Unattended -NoSound -nop4
```

UMG 에셋 패치/저장 자동화(위젯 에셋 수정, 레이아웃 변경 등)에 사용합니다.

### 3.4 전체 에디터 실행(인터랙티브)

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor.exe" "$ProjectFile"
```

UMG Designer 수동 편집이나 시각 확인이 필요할 때 사용합니다.

## 4. UMG 기준 엔드투엔드 예시

의도 명확화:
- 이 예시의 Python 커맨드릿은 실행 셸(러너) 역할입니다.
- UMG 에셋 생성/수정 로직 자체는 MCP 우선(`widget.apply_ops`, `widget.create_or_patch`)입니다.
- Python은 에디터 내부에서 MCP 도구를 호출하고 dirty 에셋을 명시적으로 저장하기 위해 사용합니다.

1. C++ 빌드:
```powershell
& "$UE\Engine\Build\BatchFiles\Build.bat" MCPDemoProjectEditor Win64 Development "$ProjectFile" -WaitMutex -FromMsBuild
```
2. UMG 에셋 패치 스크립트 실행:
```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$ProjectFile" -run=pythonscript -script=D:\github\ue5mcp-codex\apply_menu5_confirm_assets.py -NoSplash -NullRHI -Unattended -NoSound -nop4
```
3. 블루프린트 컴파일:
```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$ProjectFile" -run=CompileAllBlueprints -unattended -nop4 -nosplash -NoLogTimes
```
4. 변경 파일 확인:
```powershell
git status --short
```

## 5. 유용한 로그 경로

- 프로젝트 로그:  
  `D:\github\ue5mcp-codex\MCPDemoProject\Saved\Logs\MCPDemoProject.log`
- UnrealBuildTool 로그:  
  `C:\Users\<사용자>\AppData\Local\UnrealBuildTool\Log.txt`

빠른 검색 예시:

```powershell
rg -n "Error|Warning|CompileAllBlueprints|pythonscript" "$ProjectRoot\Saved\Logs\*.log"
rg -n "error|warning|Result:" "C:\Users\$env:USERNAME\AppData\Local\UnrealBuildTool\Log.txt"
```

## 6. 옵션 빠른 설명

- `-NullRHI`: GPU 렌더링 없이 실행(헤드리스/CI 성격 작업에 적합)
- `-Unattended`: 대화상자/입력 대기 없이 실행
- `-nop4`: Perforce 연동 비활성
- `-NoSound`: 오디오 초기화 생략으로 커맨드릿 실행 단순화

## 7. 자주 발생하는 실수

1. 프로젝트와 맞지 않는 UE 버전으로 실행.
2. 스크립트에 저장 로직 없이 커맨드릿 수정이 자동 저장될 거라고 가정.
3. C++ 빌드만 확인하고 Blueprint 컴파일 검증을 생략.
4. 커맨드릿 실행 후 `git status` 확인을 놓침.
