#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "UEMCPToolRegistry.h"

class FUEMCPImportAutomationManager;
class FUEMCPHttpBridge;
class FUEMCPNamedPipeBridge;

class UEMCPTOOLSEDITOR_API FUEMCPToolsEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FMCPToolRegistry& GetToolRegistry()
	{
		return ToolRegistry;
	}

private:
	void RegisterTools();
	void UnregisterTools();

private:
	FMCPToolRegistry ToolRegistry;
	FUEMCPImportAutomationManager* ImportAutomationManager = nullptr;
	FUEMCPHttpBridge* HttpBridge = nullptr;
	FUEMCPNamedPipeBridge* NamedPipeBridge = nullptr;
};
