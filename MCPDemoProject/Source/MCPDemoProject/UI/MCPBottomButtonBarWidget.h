// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPBottomButtonBarWidget.generated.h"

class UHorizontalBox;

UCLASS(BlueprintType, Blueprintable)
class MCPDEMOPROJECT_API UMCPBottomButtonBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Button Bar", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ButtonContainer = nullptr;
};
