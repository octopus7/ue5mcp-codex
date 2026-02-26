#include "UEMCPEditorSubsystem.h"

#include "ScopedTransaction.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UEMCPLog.h"
#include "UEMCPToolsEditorModule.h"

void UUEMCPEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogUEMCPTools, Log, TEXT("UEMCPEditorSubsystem initialized."));
}

void UUEMCPEditorSubsystem::Deinitialize()
{
	UE_LOG(LogUEMCPTools, Log, TEXT("UEMCPEditorSubsystem deinitialized."));
	Super::Deinitialize();
}

bool UUEMCPEditorSubsystem::InvokeTool(const FMCPInvokeRequest& Request, FMCPInvokeResponse& OutResponse)
{
	OutResponse = FMCPInvokeResponse();
	OutResponse.RequestId = Request.RequestId;

	if (Request.ToolId.IsEmpty())
	{
		OutResponse.Status = EMCPInvokeStatus::ValidationError;
		OutResponse.AddError(TEXT("invalid_tool"), TEXT("Request.ToolId is required."), FString(), true);
		return false;
	}

	FUEMCPToolsEditorModule* Module = FModuleManager::GetModulePtr<FUEMCPToolsEditorModule>(TEXT("UEMCPToolsEditor"));
	if (Module == nullptr)
	{
		OutResponse.Status = EMCPInvokeStatus::ExecutionError;
		OutResponse.AddError(TEXT("module_missing"), TEXT("UEMCPToolsEditor module is not loaded."), FString(), true);
		return false;
	}

	FMCPToolRegistry& Registry = Module->GetToolRegistry();
	if (!Registry.HasTool(Request.ToolId))
	{
		OutResponse.Status = EMCPInvokeStatus::UnknownTool;
		OutResponse.AddError(TEXT("unknown_tool"), FString::Printf(TEXT("Unknown tool id: %s"), *Request.ToolId), FString(), true);
		return false;
	}

	TUniquePtr<FScopedTransaction> Transaction;
	if (!Request.bDryRun)
	{
		Transaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("UEMCPTools", "InvokeToolTransaction", "UEMCP Tool Invocation"));
	}

	const bool bExecuteOk = Registry.Execute(Request, OutResponse);
	if (!bExecuteOk || OutResponse.bHasFatalError)
	{
		if (Transaction.IsValid())
		{
			Transaction->Cancel();
		}
		if (OutResponse.Status == EMCPInvokeStatus::Success)
		{
			OutResponse.Status = EMCPInvokeStatus::ExecutionError;
		}
		return false;
	}

	if (OutResponse.HasErrors())
	{
		if (Transaction.IsValid())
		{
			Transaction->Cancel();
		}
		if (OutResponse.Status == EMCPInvokeStatus::Success)
		{
			OutResponse.Status = EMCPInvokeStatus::ExecutionError;
		}
		return false;
	}

	OutResponse.Status = EMCPInvokeStatus::Success;
	return true;
}

bool UUEMCPEditorSubsystem::ParseAndInvokeJson(const FString& RequestBodyJson, FMCPInvokeResponse& OutResponse, const FString& AuthToken)
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestBodyJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutResponse = FMCPInvokeResponse();
		OutResponse.Status = EMCPInvokeStatus::ValidationError;
		OutResponse.AddError(TEXT("invalid_json"), TEXT("Failed to parse invoke JSON body."), FString(), true);
		return false;
	}

	FMCPInvokeRequest Request;
	RootObject->TryGetStringField(TEXT("request_id"), Request.RequestId);
	RootObject->TryGetStringField(TEXT("tool_id"), Request.ToolId);
	RootObject->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
	Request.AuthToken = AuthToken;

	if (RootObject->HasTypedField<EJson::Object>(TEXT("payload")))
	{
		const TSharedPtr<FJsonObject> PayloadObject = RootObject->GetObjectField(TEXT("payload"));
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Request.PayloadJson);
		FJsonSerializer::Serialize(PayloadObject.ToSharedRef(), Writer);
	}
	else if (RootObject->HasTypedField<EJson::String>(TEXT("payload_json")))
	{
		Request.PayloadJson = RootObject->GetStringField(TEXT("payload_json"));
	}

	return InvokeTool(Request, OutResponse);
}
