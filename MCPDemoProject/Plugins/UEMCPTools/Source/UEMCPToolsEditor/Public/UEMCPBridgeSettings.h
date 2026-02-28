#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UEMCPBridgeSettings.generated.h"

UCLASS(Config = Editor, DefaultConfig, meta = (DisplayName = "UE MCP Local Bridge"))
class UEMCPTOOLSEDITOR_API UUEMCPBridgeSettings : public UDeveloperSettings
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
		return NSLOCTEXT("UEMCPTools", "BridgeSettingsSection", "UE MCP Local Bridge");
	}
#endif

	UPROPERTY(EditAnywhere, Config, Category = "Bridge")
	bool bEnableBridge = true;

	UPROPERTY(EditAnywhere, Config, Category = "Bridge|HTTP")
	bool bEnableHttpBridge = true;

	UPROPERTY(EditAnywhere, Config, Category = "Bridge|HTTP", meta = (ClampMin = "1024", ClampMax = "65535"))
	int32 Port = 17777;

	UPROPERTY(EditAnywhere, Config, Category = "Bridge|HTTP")
	FString BindAddress = TEXT("127.0.0.1");

	UPROPERTY(EditAnywhere, Config, Category = "Bridge|Named Pipe")
	bool bEnableNamedPipeBridge = true;

	UPROPERTY(EditAnywhere, Config, Category = "Bridge|Named Pipe")
	FString NamedPipeName = TEXT("UEMCPToolsBridge");

	UPROPERTY(EditAnywhere, Config, Category = "Bridge|Named Pipe", meta = (ClampMin = "1024", ClampMax = "8388608"))
	int32 NamedPipeMaxMessageBytes = 1048576;

	UPROPERTY(EditAnywhere, Config, Category = "Bridge")
	bool bRequireAuthToken = true;

	UPROPERTY(EditAnywhere, Config, Category = "Bridge", meta = (PasswordField = true))
	FString AuthToken = TEXT("change-me");
};
