@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "PLUGIN_FILE=%PROJECT_DIR%Plugins\OctoMCP\OctoMCP.uplugin"
set "PACKAGE_DIR=%PROJECT_DIR%..\Artifacts\OctoMCP_Package"
set "DEFAULT_ENGINE_ROOT=C:\Program Files\Epic Games\UE_5.7\Engine"

if defined UE57_ROOT (
    set "ENGINE_ROOT=%UE57_ROOT%"
) else (
    set "ENGINE_ROOT=%DEFAULT_ENGINE_ROOT%"
)

set "RUN_UAT=%ENGINE_ROOT%\Build\BatchFiles\RunUAT.bat"

if not exist "%PLUGIN_FILE%" (
    echo [OctoMCP] Plugin file not found: "%PLUGIN_FILE%"
    exit /b 1
)

if not exist "%RUN_UAT%" (
    echo [OctoMCP] RunUAT.bat not found: "%RUN_UAT%"
    echo [OctoMCP] Set UE57_ROOT to your Unreal Engine 5.7 root directory and try again.
    exit /b 1
)

if exist "%PACKAGE_DIR%" (
    echo [OctoMCP] Removing existing package directory: "%PACKAGE_DIR%"
    rmdir /s /q "%PACKAGE_DIR%"
    if exist "%PACKAGE_DIR%" (
        echo [OctoMCP] Failed to remove existing package directory.
        exit /b 1
    )
)

echo [OctoMCP] Packaging plugin...
echo [OctoMCP] Engine: "%ENGINE_ROOT%"
echo [OctoMCP] Plugin: "%PLUGIN_FILE%"
echo [OctoMCP] Output: "%PACKAGE_DIR%"

call "%RUN_UAT%" BuildPlugin -Plugin="%PLUGIN_FILE%" -Package="%PACKAGE_DIR%" -TargetPlatforms=Win64
if errorlevel 1 (
    echo [OctoMCP] Packaging failed.
    exit /b %errorlevel%
)

echo [OctoMCP] Packaging completed successfully.
echo [OctoMCP] Package path: "%PACKAGE_DIR%"
exit /b 0
