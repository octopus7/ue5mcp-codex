#include "UEMCPImportTools.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UEMCPLog.h"

namespace
{
FString NormalizeFolderPath(const FString& InPath)
{
	FString Path = InPath;
	Path.TrimStartAndEndInline();
	if (Path.EndsWith(TEXT("/")))
	{
		Path.LeftChopInline(1, EAllowShrinking::No);
	}
	return Path;
}

bool IsFolderMatch(const FString& AssetFolder, const FMCPImportRule& Rule)
{
	if (!Rule.bEnabled)
	{
		return false;
	}

	const FString NormalizedAssetFolder = NormalizeFolderPath(AssetFolder);
	const FString RuleFolder = NormalizeFolderPath(Rule.FolderPath);
	if (RuleFolder.IsEmpty())
	{
		return false;
	}

	if (Rule.bRecursive)
	{
		return NormalizedAssetFolder == RuleFolder || NormalizedAssetFolder.StartsWith(RuleFolder + TEXT("/"));
	}

	return NormalizedAssetFolder == RuleFolder;
}

FString GetTextureFolderPath(UTexture2D* Texture)
{
	if (Texture == nullptr || Texture->GetOutermost() == nullptr)
	{
		return FString();
	}

	const FString PackageName = Texture->GetOutermost()->GetName();
	return FPackageName::GetLongPackagePath(PackageName);
}

bool ApplyPreset(UTexture2D* Texture, const FMCPUITexturePreset& Preset, bool bDryRun, FMCPInvokeResponse* Response)
{
	if (Texture == nullptr)
	{
		return false;
	}

	bool bChanged = false;
	if (Texture->CompressionSettings != Preset.CompressionSettings)
	{
		bChanged = true;
		if (!bDryRun)
		{
			Texture->CompressionSettings = Preset.CompressionSettings;
		}
	}

	if (Texture->LODGroup != Preset.LODGroup)
	{
		bChanged = true;
		if (!bDryRun)
		{
			Texture->LODGroup = Preset.LODGroup;
		}
	}

	if (Texture->MipGenSettings != Preset.MipGenSettings)
	{
		bChanged = true;
		if (!bDryRun)
		{
			Texture->MipGenSettings = Preset.MipGenSettings;
		}
	}

	if (Texture->NeverStream != Preset.bNeverStream)
	{
		bChanged = true;
		if (!bDryRun)
		{
			Texture->NeverStream = Preset.bNeverStream;
		}
	}

	if (Texture->SRGB != Preset.bSRGB)
	{
		bChanged = true;
		if (!bDryRun)
		{
			Texture->SRGB = Preset.bSRGB;
		}
	}

	if (!bDryRun && bChanged)
	{
		Texture->Modify();
		Texture->PostEditChange();
		Texture->MarkPackageDirty();
	}

	if (bChanged)
	{
		if (Response != nullptr)
		{
			Response->AppliedCount += 1;
		}
	}
	else
	{
		if (Response != nullptr)
		{
			Response->SkippedCount += 1;
		}
	}

	return bChanged;
}

bool GatherFolderPathsFromPayload(const FString& PayloadJson, TArray<FString>& OutFolderPaths, FMCPInvokeResponse& Response)
{
	if (PayloadJson.IsEmpty())
	{
		return true;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("invalid_payload"), TEXT("Failed to parse payload JSON for import tool."), FString(), true);
		return false;
	}

	if (!RootObject->HasField(TEXT("folder_paths")))
	{
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* FolderPaths = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("folder_paths"), FolderPaths) || FolderPaths == nullptr)
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("invalid_folder_paths"), TEXT("folder_paths must be an array."), FString(), true);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *FolderPaths)
	{
		if (!Value.IsValid() || Value->Type != EJson::String)
		{
			continue;
		}
		OutFolderPaths.Add(NormalizeFolderPath(Value->AsString()));
	}

	return true;
}

void GatherTexturesForFolders(const TArray<FString>& FolderPaths, TArray<FAssetData>& OutAssets)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = AssetRegistryModule.Get();

	TSet<FString> SeenObjectPaths;
	for (const FString& FolderPath : FolderPaths)
	{
		if (FolderPath.IsEmpty())
		{
			continue;
		}

		FARFilter Filter;
		Filter.PackagePaths.Add(FName(*FolderPath));
		Filter.ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
		Filter.bRecursivePaths = true;

		TArray<FAssetData> LocalAssets;
		Registry.GetAssets(Filter, LocalAssets);

		for (const FAssetData& AssetData : LocalAssets)
		{
			const FString ObjectPath = AssetData.GetSoftObjectPath().ToString();
			if (!SeenObjectPaths.Contains(ObjectPath))
			{
				SeenObjectPaths.Add(ObjectPath);
				OutAssets.Add(AssetData);
			}
		}
	}
}
}

bool FUEMCPImportTools::ExecuteApplyFolderRules(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
{
	const UUEMCPImportSettings* Settings = GetDefault<UUEMCPImportSettings>();
	if (Settings == nullptr)
	{
		Response.Status = EMCPInvokeStatus::ExecutionError;
		Response.AddError(TEXT("settings_missing"), TEXT("UUEMCPImportSettings is not available."), FString(), true);
		return false;
	}

	TArray<FString> TargetFolders;
	if (!GatherFolderPathsFromPayload(Request.PayloadJson, TargetFolders, Response))
	{
		return false;
	}

	if (TargetFolders.Num() == 0)
	{
		for (const FMCPImportRule& Rule : Settings->Rules)
		{
			if (Rule.bEnabled)
			{
				TargetFolders.Add(NormalizeFolderPath(Rule.FolderPath));
			}
		}
	}

	TargetFolders = TargetFolders.FilterByPredicate([](const FString& Path) { return !Path.IsEmpty(); });
	TSet<FString> UniqueFolders(TargetFolders);
	TargetFolders = UniqueFolders.Array();
	TargetFolders.Sort();

	TArray<FAssetData> TextureAssets;
	GatherTexturesForFolders(TargetFolders, TextureAssets);

	UE_LOG(LogUEMCPTools, Log, TEXT("Import apply rules scanned %d textures."), TextureAssets.Num());
	for (const FAssetData& AssetData : TextureAssets)
	{
		UTexture2D* Texture = Cast<UTexture2D>(AssetData.GetAsset());
		if (Texture == nullptr)
		{
			Response.SkippedCount += 1;
			continue;
		}

		ApplyRulesToTexture(Texture, Settings->Rules, Request.bDryRun, &Response);
	}

	return true;
}

bool FUEMCPImportTools::ExecuteScanAndRepair(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
{
	FMCPInvokeRequest LocalRequest = Request;
	if (LocalRequest.PayloadJson.IsEmpty())
	{
		LocalRequest.PayloadJson = TEXT("{}");
	}

	return ExecuteApplyFolderRules(LocalRequest, Response);
}

bool FUEMCPImportTools::ApplyRulesToTexture(UTexture2D* Texture, const TArray<FMCPImportRule>& Rules, bool bDryRun, FMCPInvokeResponse* Response)
{
	if (Texture == nullptr)
	{
		return false;
	}

	const FString FolderPath = GetTextureFolderPath(Texture);
	const int32 RuleIndex = FindFirstMatchingRuleIndex(FolderPath, Rules);
	if (RuleIndex == INDEX_NONE)
	{
		if (Response != nullptr)
		{
			Response->SkippedCount += 1;
		}
		return false;
	}

	return ApplyPreset(Texture, Rules[RuleIndex].Preset, bDryRun, Response);
}

int32 FUEMCPImportTools::FindFirstMatchingRuleIndex(const FString& AssetFolderPath, const TArray<FMCPImportRule>& Rules)
{
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		if (IsFolderMatch(AssetFolderPath, Rules[Index]))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
