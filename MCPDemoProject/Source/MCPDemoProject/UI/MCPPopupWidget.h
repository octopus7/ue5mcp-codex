// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPPopupWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class MCPDEMOPROJECT_API UMCPPopupWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Popup", meta = (BindWidgetOptional))
	TObjectPtr<UImage> PopupImage = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Popup", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Popup", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BodyText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Popup", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FooterText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Popup", meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton = nullptr;
};
