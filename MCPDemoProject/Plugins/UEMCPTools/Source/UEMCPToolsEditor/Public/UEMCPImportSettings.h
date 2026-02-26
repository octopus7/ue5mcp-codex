#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureDefines.h"
#include "Engine/Texture.h"
#include "Engine/DeveloperSettings.h"
#include "UEMCPImportSettings.generated.h"

USTRUCT(BlueprintType)
struct FMCPUITexturePreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "UI Texture")
	TEnumAsByte<TextureCompressionSettings> CompressionSettings = TC_EditorIcon;

	UPROPERTY(EditAnywhere, Config, Category = "UI Texture")
	TEnumAsByte<TextureGroup> LODGroup = TEXTUREGROUP_UI;

	UPROPERTY(EditAnywhere, Config, Category = "UI Texture")
	TEnumAsByte<TextureMipGenSettings> MipGenSettings = TMGS_NoMipmaps;

	UPROPERTY(EditAnywhere, Config, Category = "UI Texture")
	bool bNeverStream = true;

	UPROPERTY(EditAnywhere, Config, Category = "UI Texture")
	bool bSRGB = true;
};

USTRUCT(BlueprintType)
struct FMCPImportRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Rule")
	FString FolderPath = TEXT("/Game/UI/Textures");

	UPROPERTY(EditAnywhere, Config, Category = "Rule")
	bool bRecursive = true;

	UPROPERTY(EditAnywhere, Config, Category = "Rule")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Config, Category = "Rule")
	FMCPUITexturePreset Preset;
};

UCLASS(Config = Editor, DefaultConfig, meta = (DisplayName = "UE MCP Import Rules"))
class UEMCPTOOLSEDITOR_API UUEMCPImportSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Plugins");
	}

#if WITH_EDITOR
	virtual FText GetSectionText() const override
	{
		return NSLOCTEXT("UEMCPTools", "ImportSettingsSection", "UE MCP Import Rules");
	}
#endif

	UPROPERTY(EditAnywhere, Config, Category = "Rules")
	TArray<FMCPImportRule> Rules;
};
