#pragma once

#include "CoreMinimal.h"
#include "HttpServerRequest.h"
#include "IHttpRouter.h"

class IHttpRouter;
enum class EMCPInvokeStatus : uint8;

class FUEMCPHttpBridge
{
public:
	void Start();
	void Stop();

private:
	bool HandleInvoke(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool IsRequestAuthorized(const FHttpServerRequest& Request, FString& OutFailureReason) const;
	static FString StatusToString(EMCPInvokeStatus Status);
	void SendJsonResponse(const FHttpResultCallback& OnComplete, int32 StatusCode, const FString& JsonBody) const;

private:
	uint32 ListeningPort = 0;
	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle RouteHandle;
};
