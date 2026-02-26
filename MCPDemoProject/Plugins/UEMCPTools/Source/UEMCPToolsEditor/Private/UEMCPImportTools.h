#pragma once

#include "CoreMinimal.h"
#include "UEMCPImportSettings.h"
#include "UEMCPTypes.h"

class UTexture2D;

class FUEMCPImportTools
{
public:
	static bool ExecuteApplyFolderRules(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response);
	static bool ExecuteScanAndRepair(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response);

	static bool ApplyRulesToTexture(UTexture2D* Texture, const TArray<FMCPImportRule>& Rules, bool bDryRun, FMCPInvokeResponse* Response);
	static int32 FindFirstMatchingRuleIndex(const FString& AssetFolderPath, const TArray<FMCPImportRule>& Rules);
};
