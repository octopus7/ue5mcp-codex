#pragma once

#include "CoreMinimal.h"
#include "UEMCPTypes.h"

class FUEMCPMaterialTools
{
public:
	static bool ExecuteCreateUiGradient(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response);
	static bool ExecuteCreateMaterialInstance(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response);
	static bool ExecuteSetMaterialInstanceParams(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response);
};
