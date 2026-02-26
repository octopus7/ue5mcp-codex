#pragma once

#include "CoreMinimal.h"
#include "UEMCPTypes.h"

class FMCPToolRegistry
{
public:
	using FToolHandler = TFunction<bool(const FMCPInvokeRequest&, FMCPInvokeResponse&)>;

	void RegisterTool(const FString& ToolId, FToolHandler&& Handler);
	bool HasTool(const FString& ToolId) const;
	bool Execute(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response) const;
	TArray<FString> ListTools() const;

private:
	TMap<FString, FToolHandler> ToolHandlers;
};
