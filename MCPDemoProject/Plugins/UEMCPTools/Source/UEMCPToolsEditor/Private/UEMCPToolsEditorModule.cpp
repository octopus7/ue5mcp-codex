#include "UEMCPToolsEditorModule.h"

#include "UEMCPImportAutomationManager.h"
#include "UEMCPImportTools.h"
#include "UEMCPHttpBridge.h"
#include "UEMCPLog.h"
#include "UEMCPMaterialTools.h"
#include "UEMCPToolIds.h"
#include "UEMCPWidgetTools.h"

void FUEMCPToolsEditorModule::StartupModule()
{
	RegisterTools();

	ImportAutomationManager = new FUEMCPImportAutomationManager();
	ImportAutomationManager->Start();

	HttpBridge = new FUEMCPHttpBridge();
	HttpBridge->Start();

	UE_LOG(LogUEMCPTools, Log, TEXT("UEMCPToolsEditor started."));
}

void FUEMCPToolsEditorModule::ShutdownModule()
{
	if (HttpBridge != nullptr)
	{
		HttpBridge->Stop();
		delete HttpBridge;
		HttpBridge = nullptr;
	}

	if (ImportAutomationManager != nullptr)
	{
		ImportAutomationManager->Stop();
		delete ImportAutomationManager;
		ImportAutomationManager = nullptr;
	}

	UnregisterTools();
	UE_LOG(LogUEMCPTools, Log, TEXT("UEMCPToolsEditor shut down."));
}

void FUEMCPToolsEditorModule::RegisterTools()
{
	ToolRegistry.RegisterTool(
		UEMCPToolIds::WidgetCreateOrPatch,
		[](const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
		{
			return FUEMCPWidgetTools::ExecuteCreateOrPatch(Request, Response);
		});

	ToolRegistry.RegisterTool(
		UEMCPToolIds::WidgetApplyOps,
		[](const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
		{
			return FUEMCPWidgetTools::ExecuteApplyOps(Request, Response);
		});

	ToolRegistry.RegisterTool(
		UEMCPToolIds::WidgetBindRefsAndEvents,
		[](const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
		{
			return FUEMCPWidgetTools::ExecuteBindRefsAndEvents(Request, Response);
		});

	ToolRegistry.RegisterTool(
		UEMCPToolIds::WidgetRepairTree,
		[](const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
		{
			return FUEMCPWidgetTools::ExecuteRepairTree(Request, Response);
		});

	ToolRegistry.RegisterTool(
		UEMCPToolIds::MaterialCreateUiGradient,
		[](const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
		{
			return FUEMCPMaterialTools::ExecuteCreateUiGradient(Request, Response);
		});

	ToolRegistry.RegisterTool(
		UEMCPToolIds::MaterialInstanceCreate,
		[](const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
		{
			return FUEMCPMaterialTools::ExecuteCreateMaterialInstance(Request, Response);
		});

	ToolRegistry.RegisterTool(
		UEMCPToolIds::MaterialInstanceSetParams,
		[](const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
		{
			return FUEMCPMaterialTools::ExecuteSetMaterialInstanceParams(Request, Response);
		});

	ToolRegistry.RegisterTool(
		UEMCPToolIds::ImportApplyFolderRules,
		[](const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
		{
			return FUEMCPImportTools::ExecuteApplyFolderRules(Request, Response);
		});

	ToolRegistry.RegisterTool(
		UEMCPToolIds::ImportScanAndRepair,
		[](const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
		{
			return FUEMCPImportTools::ExecuteScanAndRepair(Request, Response);
		});
}

void FUEMCPToolsEditorModule::UnregisterTools()
{
	ToolRegistry = FMCPToolRegistry();
}

IMPLEMENT_MODULE(FUEMCPToolsEditorModule, UEMCPToolsEditor);
