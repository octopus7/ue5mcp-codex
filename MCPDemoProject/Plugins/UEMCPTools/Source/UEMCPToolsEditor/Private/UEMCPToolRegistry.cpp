#include "UEMCPToolRegistry.h"

void FMCPToolRegistry::RegisterTool(const FString& ToolId, FToolHandler&& Handler)
{
	ToolHandlers.Add(ToolId, MoveTemp(Handler));
}

bool FMCPToolRegistry::HasTool(const FString& ToolId) const
{
	return ToolHandlers.Contains(ToolId);
}

bool FMCPToolRegistry::Execute(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response) const
{
	const FToolHandler* FoundHandler = ToolHandlers.Find(Request.ToolId);
	if (FoundHandler == nullptr)
	{
		Response.Status = EMCPInvokeStatus::UnknownTool;
		Response.AddError(TEXT("unknown_tool"), FString::Printf(TEXT("Unknown tool id: %s"), *Request.ToolId), FString(), true);
		return false;
	}

	return (*FoundHandler)(Request, Response);
}

TArray<FString> FMCPToolRegistry::ListTools() const
{
	TArray<FString> Keys;
	ToolHandlers.GetKeys(Keys);
	Keys.Sort();
	return Keys;
}
