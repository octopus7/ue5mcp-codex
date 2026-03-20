// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPBottomButtonBarWidget.generated.h"

class UButton;
class UHorizontalBox;
class UMCPPopupWidget;

UCLASS(BlueprintType, Blueprintable)
class MCPDEMOPROJECT_API UMCPBottomButtonBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleTestPopupOpenButtonClicked();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Button Bar")
	TSubclassOf<UMCPPopupWidget> PopupWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "Button Bar", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ButtonContainer = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Button Bar", meta = (BindWidgetOptional))
	TObjectPtr<UButton> TestPopupOpenButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMCPPopupWidget> ActivePopupWidget = nullptr;
};
