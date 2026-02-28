#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

enum class EMCPInvokeStatus : uint8;
struct FMCPInvokeResponse;

class FUEMCPNamedPipeBridge
{
public:
	void Start();
	void Stop();

private:
	static FString StatusToString(EMCPInvokeStatus Status);

#if PLATFORM_WINDOWS
	void RunServerLoop();
	void WakeServerLoop() const;

	bool HandleClient(void* PipeHandle) const;
	bool ReadExact(void* PipeHandle, uint8* Buffer, uint32 ByteCount) const;
	bool WriteExact(void* PipeHandle, const uint8* Buffer, uint32 ByteCount) const;

	bool ReadRequestMessage(void* PipeHandle, FString& OutRequestBody) const;
	bool WriteResponseMessage(void* PipeHandle, const FString& ResponseBody) const;

	bool IsRequestAuthorized(const TSharedPtr<FJsonObject>& RequestObject, FString& OutAuthToken, FString& OutFailureReason) const;
	void InvokeToolOnGameThread(const struct FMCPInvokeRequest& Request, FMCPInvokeResponse& OutResponse) const;
	FString BuildInvokeResponseJson(const FMCPInvokeResponse& Response) const;
	FString BuildErrorResponseJson(const FString& RequestId, const FString& Status, const FString& ErrorMessage) const;

	FThreadSafeBool bStopRequested = false;
	bool bWorkerRunning = false;
	TFuture<void> WorkerFuture;
	FString PipeName;
	FString FullPipePath;
	int32 MaxMessageBytes = 1024 * 1024;
#endif
};
