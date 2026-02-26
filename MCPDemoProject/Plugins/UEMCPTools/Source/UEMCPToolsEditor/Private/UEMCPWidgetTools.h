#pragma once

#include "CoreMinimal.h"
#include "UEMCPTypes.h"

class FUEMCPWidgetTools
{
public:
	static bool ExecuteCreateOrPatch(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response);
	static bool ExecuteApplyOps(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response);
	static bool ExecuteBindRefsAndEvents(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response);
	static bool ExecuteRepairTree(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response);
};
