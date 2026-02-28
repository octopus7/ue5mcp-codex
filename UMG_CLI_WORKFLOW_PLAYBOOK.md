# UE5 UMG Command-Line Workflow Playbook

This document is a practical command-line guide for common UMG workflows in Unreal Engine 5 (Windows + PowerShell).

## 1. Prerequisites

- Unreal Engine installed (example: `E:\WorkTemp\Epic Games\UE_5.7`)
- Project file exists (example: `D:\github\ue5mcp-codex\MCPDemoProject\MCPDemoProject.uproject`)
- PowerShell terminal

## 2. Recommended Variables

```powershell
$ProjectRoot = "D:\github\ue5mcp-codex\MCPDemoProject"
$ProjectFile = "$ProjectRoot\MCPDemoProject.uproject"
$UE = "E:\WorkTemp\Epic Games\UE_5.7"
```

## 3. Core Commands

### 3.1 Build C++ Editor Target

```powershell
& "$UE\Engine\Build\BatchFiles\Build.bat" `
  MCPDemoProjectEditor Win64 Development `
  "$ProjectFile" -WaitMutex -FromMsBuild
```

Use when you changed C++ code used by UMG widgets/subsystems.

### 3.2 Compile All Blueprints (Headless)

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$ProjectFile" `
  -run=CompileAllBlueprints -unattended -nop4 -nosplash -NoLogTimes
```

Use to quickly validate Blueprint compile health after UMG changes.

### 3.3 Run a Python Script in Editor Commandlet

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$ProjectFile" `
  -run=pythonscript -script=D:\path\to\script.py `
  -NoSplash -NullRHI -Unattended -NoSound -nop4
```

Use for scripted asset patching/saving (UMG widget assets, layout updates, etc.).

### 3.4 Open Full Editor (Interactive)

```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor.exe" "$ProjectFile"
```

Use when you need manual UMG Designer edits or in-editor visual checks.

## 4. UMG-Focused End-to-End Example

Important intent note:
- The Python commandlet in this example is a runner/shell.
- The UMG asset creation/update logic is still MCP-first (`widget.apply_ops` / `widget.create_or_patch`).
- Python is used here to invoke MCP tools inside the editor and to explicitly save dirty assets.

1. Build C++:
```powershell
& "$UE\Engine\Build\BatchFiles\Build.bat" MCPDemoProjectEditor Win64 Development "$ProjectFile" -WaitMutex -FromMsBuild
```
2. Run UMG asset patch script:
```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$ProjectFile" -run=pythonscript -script=D:\github\ue5mcp-codex\apply_menu5_confirm_assets.py -NoSplash -NullRHI -Unattended -NoSound -nop4
```
3. Compile Blueprints:
```powershell
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$ProjectFile" -run=CompileAllBlueprints -unattended -nop4 -nosplash -NoLogTimes
```
4. Verify changed assets/code:
```powershell
git status --short
```

## 5. Useful Log Locations

- Project log:  
  `D:\github\ue5mcp-codex\MCPDemoProject\Saved\Logs\MCPDemoProject.log`
- UnrealBuildTool log:  
  `C:\Users\<YourUser>\AppData\Local\UnrealBuildTool\Log.txt`

Quick search examples:

```powershell
rg -n "Error|Warning|CompileAllBlueprints|pythonscript" "$ProjectRoot\Saved\Logs\*.log"
rg -n "error|warning|Result:" "C:\Users\$env:USERNAME\AppData\Local\UnrealBuildTool\Log.txt"
```

## 6. Option Quick Notes

- `-NullRHI`: no GPU rendering, good for headless/CI-like runs
- `-Unattended`: no interactive prompts
- `-nop4`: disable Perforce integration
- `-NoSound`: skip audio init, faster/cleaner commandlet runs

## 7. Common Pitfalls

1. Using the wrong engine version for the project.
2. Assuming commandlet modifications are saved without explicit save logic in script.
3. Validating only C++ build and skipping Blueprint compilation.
4. Forgetting `git status` after asset commandlets.
