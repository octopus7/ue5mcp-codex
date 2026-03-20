// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MCPDemoGameMode.generated.h"

class UMCPBottomButtonBarWidget;

UCLASS(BlueprintType, Blueprintable)
class MCPDEMOPROJECT_API AMCPDemoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void StartPlay() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UMCPBottomButtonBarWidget> BottomButtonBarWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPBottomButtonBarWidget> BottomButtonBarWidgetInstance = nullptr;
};
