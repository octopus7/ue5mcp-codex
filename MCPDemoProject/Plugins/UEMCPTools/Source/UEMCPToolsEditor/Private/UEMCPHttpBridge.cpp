#include "UEMCPHttpBridge.h"

#include "Editor.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Misc/Guid.h"
#include "Containers/StringConv.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UEMCPBridgeSettings.h"
#include "UEMCPEditorSubsystem.h"
#include "UEMCPLog.h"
#include "UEMCPTypes.h"

namespace
{
int32 ToHttpStatusCode(EMCPInvokeStatus Status)
{
	switch (Status)
	{
	case EMCPInvokeStatus::Success:
		return 200;
	case EMCPInvokeStatus::ValidationError:
		return 400;
	case EMCPInvokeStatus::Unauthorized:
		return 401;
	case EMCPInvokeStatus::UnknownTool:
		return 404;
	case EMCPInvokeStatus::ExecutionError:
	default:
		return 500;
	}
}

EHttpServerResponseCodes ToHttpServerResponseCode(int32 Code)
{
	return static_cast<EHttpServerResponseCodes>(Code);
}

FString ParseRequestBodyUtf8(const TArray<uint8>& Body)
{
	if (Body.Num() == 0)
	{
		return FString();
	}

	const ANSICHAR* DataPtr = reinterpret_cast<const ANSICHAR*>(Body.GetData());
	FUTF8ToTCHAR Utf8Converter(DataPtr, Body.Num());
	return FString(Utf8Converter.Length(), Utf8Converter.Get());
}

void AddMessagesAsJsonArray(TSharedPtr<FJsonObject> RootObject, const TCHAR* FieldName, const TArray<FMCPExecutionMessage>& Messages)
{
	TArray<TSharedPtr<FJsonValue>> JsonMessages;
	for (const FMCPExecutionMessage& Message : Messages)
	{
		TSharedPtr<FJsonObject> MessageObject = MakeShared<FJsonObject>();
		MessageObject->SetStringField(TEXT("code"), Message.Code);
		MessageObject->SetStringField(TEXT("message"), Message.Message);
		MessageObject->SetStringField(TEXT("detail"), Message.Detail);
		JsonMessages.Add(MakeShared<FJsonValueObject>(MessageObject));
	}

	RootObject->SetArrayField(FieldName, JsonMessages);
}

FString SerializeJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return Output;
}
}

void FUEMCPHttpBridge::Start()
{
	const UUEMCPBridgeSettings* Settings = GetDefault<UUEMCPBridgeSettings>();
	if (Settings == nullptr || !Settings->bEnableBridge)
	{
		UE_LOG(LogUEMCPTools, Log, TEXT("MCP local bridge is disabled."));
		return;
	}

	ListeningPort = static_cast<uint32>(FMath::Clamp(Settings->Port, 1024, 65535));

	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	Router = HttpServerModule.GetHttpRouter(ListeningPort);
	if (!Router.IsValid())
	{
		UE_LOG(LogUEMCPTools, Error, TEXT("Failed to acquire HTTP router on port %u."), ListeningPort);
		return;
	}

	RouteHandle = Router->BindRoute(
		FHttpPath(TEXT("/mcp/v1/invoke")),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FUEMCPHttpBridge::HandleInvoke));

	HttpServerModule.StartAllListeners();
	UE_LOG(LogUEMCPTools, Log, TEXT("MCP local bridge listening on 127.0.0.1:%u"), ListeningPort);
}

void FUEMCPHttpBridge::Stop()
{
	if (!Router.IsValid())
	{
		return;
	}

	Router->UnbindRoute(RouteHandle);
	RouteHandle.Reset();
	Router.Reset();

	FHttpServerModule::Get().StopAllListeners();
	UE_LOG(LogUEMCPTools, Log, TEXT("MCP local bridge stopped."));
}

bool FUEMCPHttpBridge::HandleInvoke(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FString AuthorizationFailureReason;
	if (!IsRequestAuthorized(Request, AuthorizationFailureReason))
	{
		TSharedPtr<FJsonObject> ErrorBody = MakeShared<FJsonObject>();
		ErrorBody->SetStringField(TEXT("status"), TEXT("Unauthorized"));
		ErrorBody->SetStringField(TEXT("error"), AuthorizationFailureReason);
		SendJsonResponse(OnComplete, 401, SerializeJsonObject(ErrorBody));
		return true;
	}

	const FString RequestBody = ParseRequestBodyUtf8(Request.Body);
	TSharedPtr<FJsonObject> RequestObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestBody);
	if (!FJsonSerializer::Deserialize(Reader, RequestObject) || !RequestObject.IsValid())
	{
		TSharedPtr<FJsonObject> ErrorBody = MakeShared<FJsonObject>();
		ErrorBody->SetStringField(TEXT("status"), TEXT("ValidationError"));
		ErrorBody->SetStringField(TEXT("error"), TEXT("Invalid JSON request body."));
		SendJsonResponse(OnComplete, 400, SerializeJsonObject(ErrorBody));
		return true;
	}

	FMCPInvokeRequest InvokeRequest;
	RequestObject->TryGetStringField(TEXT("request_id"), InvokeRequest.RequestId);
	RequestObject->TryGetStringField(TEXT("tool_id"), InvokeRequest.ToolId);
	RequestObject->TryGetBoolField(TEXT("dry_run"), InvokeRequest.bDryRun);

	if (RequestObject->HasTypedField<EJson::Object>(TEXT("payload")))
	{
		const TSharedPtr<FJsonObject> PayloadObject = RequestObject->GetObjectField(TEXT("payload"));
		InvokeRequest.PayloadJson = SerializeJsonObject(PayloadObject);
	}
	else
	{
		RequestObject->TryGetStringField(TEXT("payload_json"), InvokeRequest.PayloadJson);
	}

	if (InvokeRequest.RequestId.IsEmpty())
	{
		InvokeRequest.RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	FMCPInvokeResponse InvokeResponse;
	if (GEditor == nullptr)
	{
		InvokeResponse.RequestId = InvokeRequest.RequestId;
		InvokeResponse.Status = EMCPInvokeStatus::ExecutionError;
		InvokeResponse.AddError(TEXT("editor_unavailable"), TEXT("GEditor is null."), FString(), true);
	}
	else
	{
		UUEMCPEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UUEMCPEditorSubsystem>();
		if (Subsystem == nullptr)
		{
			InvokeResponse.RequestId = InvokeRequest.RequestId;
			InvokeResponse.Status = EMCPInvokeStatus::ExecutionError;
			InvokeResponse.AddError(TEXT("subsystem_unavailable"), TEXT("UEMCPEditorSubsystem is unavailable."), FString(), true);
		}
		else
		{
			Subsystem->InvokeTool(InvokeRequest, InvokeResponse);
		}
	}

	TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
	ResponseObject->SetStringField(TEXT("request_id"), InvokeResponse.RequestId);
	ResponseObject->SetStringField(TEXT("status"), StatusToString(InvokeResponse.Status));
	ResponseObject->SetNumberField(TEXT("applied_count"), InvokeResponse.AppliedCount);
	ResponseObject->SetNumberField(TEXT("skipped_count"), InvokeResponse.SkippedCount);

	AddMessagesAsJsonArray(ResponseObject, TEXT("warnings"), InvokeResponse.Warnings);
	AddMessagesAsJsonArray(ResponseObject, TEXT("errors"), InvokeResponse.Errors);

	if (!InvokeResponse.DiffJson.IsEmpty())
	{
		TSharedPtr<FJsonObject> DiffObject;
		const TSharedRef<TJsonReader<>> DiffReader = TJsonReaderFactory<>::Create(InvokeResponse.DiffJson);
		if (FJsonSerializer::Deserialize(DiffReader, DiffObject) && DiffObject.IsValid())
		{
			ResponseObject->SetObjectField(TEXT("diff"), DiffObject);
		}
		else
		{
			ResponseObject->SetStringField(TEXT("diff_raw"), InvokeResponse.DiffJson);
		}
	}

	SendJsonResponse(OnComplete, ToHttpStatusCode(InvokeResponse.Status), SerializeJsonObject(ResponseObject));
	return true;
}

bool FUEMCPHttpBridge::IsRequestAuthorized(const FHttpServerRequest& Request, FString& OutFailureReason) const
{
	const UUEMCPBridgeSettings* Settings = GetDefault<UUEMCPBridgeSettings>();
	if (Settings == nullptr)
	{
		OutFailureReason = TEXT("Bridge settings unavailable.");
		return false;
	}

	if (!Settings->BindAddress.Equals(TEXT("127.0.0.1")))
	{
		UE_LOG(LogUEMCPTools, Warning, TEXT("BindAddress is set to %s but the bridge is intended for localhost-only usage."), *Settings->BindAddress);
	}

	if (!Settings->bRequireAuthToken)
	{
		return true;
	}

	const TArray<FString>* AuthorizationHeaderValues = Request.Headers.Find(TEXT("Authorization"));
	if (AuthorizationHeaderValues == nullptr || AuthorizationHeaderValues->Num() == 0)
	{
		OutFailureReason = TEXT("Missing Authorization header.");
		return false;
	}

	const FString ExpectedHeader = FString::Printf(TEXT("Bearer %s"), *Settings->AuthToken);
	const FString& AuthorizationHeader = (*AuthorizationHeaderValues)[0];
	if (!AuthorizationHeader.Equals(ExpectedHeader, ESearchCase::CaseSensitive))
	{
		OutFailureReason = TEXT("Invalid authorization token.");
		return false;
	}

	return true;
}

FString FUEMCPHttpBridge::StatusToString(EMCPInvokeStatus Status)
{
	switch (Status)
	{
	case EMCPInvokeStatus::Success:
		return TEXT("Success");
	case EMCPInvokeStatus::ValidationError:
		return TEXT("ValidationError");
	case EMCPInvokeStatus::ExecutionError:
		return TEXT("ExecutionError");
	case EMCPInvokeStatus::Unauthorized:
		return TEXT("Unauthorized");
	case EMCPInvokeStatus::UnknownTool:
		return TEXT("UnknownTool");
	default:
		return TEXT("Unknown");
	}
}

void FUEMCPHttpBridge::SendJsonResponse(const FHttpResultCallback& OnComplete, int32 StatusCode, const FString& JsonBody) const
{
	TUniquePtr<FHttpServerResponse> HttpResponse = FHttpServerResponse::Create(JsonBody, TEXT("application/json"));
	HttpResponse->Code = ToHttpServerResponseCode(StatusCode);
	OnComplete(MoveTemp(HttpResponse));
}
