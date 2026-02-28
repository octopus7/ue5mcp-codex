#include "UEMCPNamedPipeBridge.h"

#if PLATFORM_WINDOWS

#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "Editor.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UEMCPBridgeSettings.h"
#include "UEMCPEditorSubsystem.h"
#include "UEMCPLog.h"
#include "UEMCPTypes.h"
#include "Windows/WindowsHWrapper.h"

namespace
{
FString SerializeJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return Output;
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
}

void FUEMCPNamedPipeBridge::Start()
{
	const UUEMCPBridgeSettings* Settings = GetDefault<UUEMCPBridgeSettings>();
	if (Settings == nullptr || !Settings->bEnableBridge || !Settings->bEnableNamedPipeBridge)
	{
		UE_LOG(LogUEMCPTools, Log, TEXT("MCP named pipe bridge is disabled."));
		return;
	}

	if (bWorkerRunning)
	{
		return;
	}

	PipeName = Settings->NamedPipeName.TrimStartAndEnd();
	if (PipeName.IsEmpty())
	{
		PipeName = TEXT("UEMCPToolsBridge");
	}

	if (PipeName.StartsWith(TEXT("\\\\.\\pipe\\"), ESearchCase::IgnoreCase))
	{
		FullPipePath = PipeName;
	}
	else
	{
		FullPipePath = FString::Printf(TEXT("\\\\.\\pipe\\%s"), *PipeName);
	}

	MaxMessageBytes = FMath::Clamp(Settings->NamedPipeMaxMessageBytes, 1024, 8 * 1024 * 1024);
	bStopRequested.AtomicSet(false);
	bWorkerRunning = true;
	WorkerFuture = Async(EAsyncExecution::Thread, [this]()
	{
		RunServerLoop();
	});

	UE_LOG(LogUEMCPTools, Log, TEXT("MCP named pipe bridge listening on %s"), *FullPipePath);
}

void FUEMCPNamedPipeBridge::Stop()
{
	if (!bWorkerRunning)
	{
		return;
	}

	bStopRequested.AtomicSet(true);
	WakeServerLoop();

	WorkerFuture.Wait();
	bWorkerRunning = false;

	UE_LOG(LogUEMCPTools, Log, TEXT("MCP named pipe bridge stopped."));
}

void FUEMCPNamedPipeBridge::RunServerLoop()
{
	while (!bStopRequested)
	{
		const FString PipePathCopy = FullPipePath;
		const uint32 OpenMode = PIPE_ACCESS_DUPLEX;
		const uint32 PipeMode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;

		HANDLE PipeHandle = CreateNamedPipeW(
			*PipePathCopy,
			OpenMode,
			PipeMode,
			PIPE_UNLIMITED_INSTANCES,
			static_cast<DWORD>(MaxMessageBytes + static_cast<int32>(sizeof(uint32))),
			static_cast<DWORD>(MaxMessageBytes + static_cast<int32>(sizeof(uint32))),
			0,
			nullptr);

		if (PipeHandle == INVALID_HANDLE_VALUE)
		{
			const uint32 ErrorCode = GetLastError();
			UE_LOG(LogUEMCPTools, Error, TEXT("CreateNamedPipeW failed for %s (Error=%u)."), *PipePathCopy, ErrorCode);
			FPlatformProcess::Sleep(0.2f);
			continue;
		}

		const bool bConnected = (ConnectNamedPipe(PipeHandle, nullptr) != 0) || (GetLastError() == ERROR_PIPE_CONNECTED);
		if (bConnected && !bStopRequested)
		{
			HandleClient(PipeHandle);
		}

		FlushFileBuffers(PipeHandle);
		DisconnectNamedPipe(PipeHandle);
		CloseHandle(PipeHandle);
	}
}

void FUEMCPNamedPipeBridge::WakeServerLoop() const
{
	if (FullPipePath.IsEmpty())
	{
		return;
	}

	HANDLE ClientHandle = CreateFileW(
		*FullPipePath,
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr);

	if (ClientHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(ClientHandle);
	}
}

bool FUEMCPNamedPipeBridge::HandleClient(void* PipeHandle) const
{
	FString RequestBody;
	if (!ReadRequestMessage(PipeHandle, RequestBody))
	{
		const FString ErrorJson = BuildErrorResponseJson(FString(), TEXT("ValidationError"), TEXT("Failed to read named pipe request body."));
		WriteResponseMessage(PipeHandle, ErrorJson);
		return false;
	}

	TSharedPtr<FJsonObject> RequestObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestBody);
	if (!FJsonSerializer::Deserialize(Reader, RequestObject) || !RequestObject.IsValid())
	{
		const FString ErrorJson = BuildErrorResponseJson(FString(), TEXT("ValidationError"), TEXT("Invalid JSON request body."));
		WriteResponseMessage(PipeHandle, ErrorJson);
		return false;
	}

	FMCPInvokeRequest InvokeRequest;
	RequestObject->TryGetStringField(TEXT("request_id"), InvokeRequest.RequestId);
	RequestObject->TryGetStringField(TEXT("tool_id"), InvokeRequest.ToolId);
	RequestObject->TryGetBoolField(TEXT("dry_run"), InvokeRequest.bDryRun);

	FString AuthFailureReason;
	if (!IsRequestAuthorized(RequestObject, InvokeRequest.AuthToken, AuthFailureReason))
	{
		const FString ErrorJson = BuildErrorResponseJson(InvokeRequest.RequestId, TEXT("Unauthorized"), AuthFailureReason);
		WriteResponseMessage(PipeHandle, ErrorJson);
		return true;
	}

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
	InvokeToolOnGameThread(InvokeRequest, InvokeResponse);

	const FString ResponseJson = BuildInvokeResponseJson(InvokeResponse);
	WriteResponseMessage(PipeHandle, ResponseJson);
	return true;
}

bool FUEMCPNamedPipeBridge::ReadExact(void* PipeHandle, uint8* Buffer, uint32 ByteCount) const
{
	HANDLE Handle = reinterpret_cast<HANDLE>(PipeHandle);
	uint32 TotalRead = 0;
	while (TotalRead < ByteCount)
	{
		DWORD ReadNow = 0;
		const BOOL bReadOk = ReadFile(Handle, Buffer + TotalRead, ByteCount - TotalRead, &ReadNow, nullptr);
		if (!bReadOk || ReadNow == 0)
		{
			return false;
		}

		TotalRead += ReadNow;
	}

	return true;
}

bool FUEMCPNamedPipeBridge::WriteExact(void* PipeHandle, const uint8* Buffer, uint32 ByteCount) const
{
	HANDLE Handle = reinterpret_cast<HANDLE>(PipeHandle);
	uint32 TotalWritten = 0;
	while (TotalWritten < ByteCount)
	{
		DWORD WroteNow = 0;
		const BOOL bWriteOk = WriteFile(Handle, Buffer + TotalWritten, ByteCount - TotalWritten, &WroteNow, nullptr);
		if (!bWriteOk || WroteNow == 0)
		{
			return false;
		}

		TotalWritten += WroteNow;
	}

	return true;
}

bool FUEMCPNamedPipeBridge::ReadRequestMessage(void* PipeHandle, FString& OutRequestBody) const
{
	uint32 PayloadBytes = 0;
	if (!ReadExact(PipeHandle, reinterpret_cast<uint8*>(&PayloadBytes), sizeof(uint32)))
	{
		return false;
	}

	if (PayloadBytes == 0 || PayloadBytes > static_cast<uint32>(MaxMessageBytes))
	{
		return false;
	}

	TArray<uint8> PayloadBuffer;
	PayloadBuffer.SetNumUninitialized(static_cast<int32>(PayloadBytes));
	if (!ReadExact(PipeHandle, PayloadBuffer.GetData(), PayloadBytes))
	{
		return false;
	}

	const ANSICHAR* DataPtr = reinterpret_cast<const ANSICHAR*>(PayloadBuffer.GetData());
	FUTF8ToTCHAR Utf8Converter(DataPtr, static_cast<int32>(PayloadBytes));
	OutRequestBody = FString(Utf8Converter.Length(), Utf8Converter.Get());
	return true;
}

bool FUEMCPNamedPipeBridge::WriteResponseMessage(void* PipeHandle, const FString& ResponseBody) const
{
	FTCHARToUTF8 Utf8(*ResponseBody);
	const uint32 PayloadBytes = static_cast<uint32>(Utf8.Length());
	if (PayloadBytes > static_cast<uint32>(MaxMessageBytes))
	{
		return false;
	}

	if (!WriteExact(PipeHandle, reinterpret_cast<const uint8*>(&PayloadBytes), sizeof(uint32)))
	{
		return false;
	}

	if (PayloadBytes > 0)
	{
		const uint8* PayloadPtr = reinterpret_cast<const uint8*>(Utf8.Get());
		if (!WriteExact(PipeHandle, PayloadPtr, PayloadBytes))
		{
			return false;
		}
	}

	return true;
}

bool FUEMCPNamedPipeBridge::IsRequestAuthorized(const TSharedPtr<FJsonObject>& RequestObject, FString& OutAuthToken, FString& OutFailureReason) const
{
	const UUEMCPBridgeSettings* Settings = GetDefault<UUEMCPBridgeSettings>();
	if (Settings == nullptr)
	{
		OutFailureReason = TEXT("Bridge settings unavailable.");
		return false;
	}

	if (!Settings->bRequireAuthToken)
	{
		return true;
	}

	RequestObject->TryGetStringField(TEXT("auth_token"), OutAuthToken);
	if (OutAuthToken.IsEmpty())
	{
		FString AuthorizationHeader;
		RequestObject->TryGetStringField(TEXT("authorization"), AuthorizationHeader);
		const FString BearerPrefix = TEXT("Bearer ");
		if (AuthorizationHeader.StartsWith(BearerPrefix))
		{
			OutAuthToken = AuthorizationHeader.RightChop(BearerPrefix.Len());
		}
	}

	if (OutAuthToken.IsEmpty())
	{
		OutFailureReason = TEXT("Missing auth_token in named pipe request.");
		return false;
	}

	if (!OutAuthToken.Equals(Settings->AuthToken, ESearchCase::CaseSensitive))
	{
		OutFailureReason = TEXT("Invalid authorization token.");
		return false;
	}

	return true;
}

void FUEMCPNamedPipeBridge::InvokeToolOnGameThread(const FMCPInvokeRequest& Request, FMCPInvokeResponse& OutResponse) const
{
	FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool(true);

	AsyncTask(ENamedThreads::GameThread, [&Request, &OutResponse, CompletionEvent]()
	{
		if (GEditor == nullptr)
		{
			OutResponse = FMCPInvokeResponse();
			OutResponse.RequestId = Request.RequestId;
			OutResponse.Status = EMCPInvokeStatus::ExecutionError;
			OutResponse.AddError(TEXT("editor_unavailable"), TEXT("GEditor is null."), FString(), true);
			CompletionEvent->Trigger();
			return;
		}

		UUEMCPEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UUEMCPEditorSubsystem>();
		if (Subsystem == nullptr)
		{
			OutResponse = FMCPInvokeResponse();
			OutResponse.RequestId = Request.RequestId;
			OutResponse.Status = EMCPInvokeStatus::ExecutionError;
			OutResponse.AddError(TEXT("subsystem_unavailable"), TEXT("UEMCPEditorSubsystem is unavailable."), FString(), true);
			CompletionEvent->Trigger();
			return;
		}

		Subsystem->InvokeTool(Request, OutResponse);
		CompletionEvent->Trigger();
	});

	CompletionEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
}

FString FUEMCPNamedPipeBridge::BuildInvokeResponseJson(const FMCPInvokeResponse& Response) const
{
	TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
	ResponseObject->SetStringField(TEXT("request_id"), Response.RequestId);
	ResponseObject->SetStringField(TEXT("status"), StatusToString(Response.Status));
	ResponseObject->SetNumberField(TEXT("applied_count"), Response.AppliedCount);
	ResponseObject->SetNumberField(TEXT("skipped_count"), Response.SkippedCount);

	AddMessagesAsJsonArray(ResponseObject, TEXT("warnings"), Response.Warnings);
	AddMessagesAsJsonArray(ResponseObject, TEXT("errors"), Response.Errors);

	if (!Response.DiffJson.IsEmpty())
	{
		TSharedPtr<FJsonObject> DiffObject;
		const TSharedRef<TJsonReader<>> DiffReader = TJsonReaderFactory<>::Create(Response.DiffJson);
		if (FJsonSerializer::Deserialize(DiffReader, DiffObject) && DiffObject.IsValid())
		{
			ResponseObject->SetObjectField(TEXT("diff"), DiffObject);
		}
		else
		{
			ResponseObject->SetStringField(TEXT("diff_raw"), Response.DiffJson);
		}
	}

	return SerializeJsonObject(ResponseObject);
}

FString FUEMCPNamedPipeBridge::BuildErrorResponseJson(const FString& RequestId, const FString& Status, const FString& ErrorMessage) const
{
	TSharedPtr<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
	if (!RequestId.IsEmpty())
	{
		ErrorObject->SetStringField(TEXT("request_id"), RequestId);
	}

	ErrorObject->SetStringField(TEXT("status"), Status);
	ErrorObject->SetStringField(TEXT("error"), ErrorMessage);
	return SerializeJsonObject(ErrorObject);
}

FString FUEMCPNamedPipeBridge::StatusToString(EMCPInvokeStatus Status)
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

#else

void FUEMCPNamedPipeBridge::Start()
{
	UE_LOG(LogUEMCPTools, Warning, TEXT("Named pipe bridge is only supported on Windows."));
}

void FUEMCPNamedPipeBridge::Stop()
{
}

FString FUEMCPNamedPipeBridge::StatusToString(EMCPInvokeStatus Status)
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

#endif
