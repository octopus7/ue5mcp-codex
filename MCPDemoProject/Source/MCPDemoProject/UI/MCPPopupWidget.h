// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "MCPPopupWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UWidget;

UCLASS(BlueprintType, Blueprintable)
class MCPDEMOPROJECT_API UMCPPopupWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleCloseButtonClicked();

	void StartOpenElasticScaleAnimation();
	void StopOpenElasticScaleAnimation();
	void HandleOpenElasticScaleAnimationTick();
	UWidget* ResolveOpenElasticScaleTargetWidget() const;
	float EvaluateElasticOutAlpha(float Alpha) const;
	void ApplyOpenElasticScale(float Scale) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Popup|Animation")
	bool bPlayOpenElasticScaleAnimation = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Popup|Animation", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float OpenElasticScaleDuration = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Popup|Animation", meta = (ClampMin = "0.10", ClampMax = "1.00", UIMin = "0.10", UIMax = "1.00"))
	float OpenElasticStartScale = 0.82f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Popup|Animation", meta = (ClampMin = "0.50", UIMin = "0.50"))
	float OpenElasticOscillationCount = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Popup|Animation")
	FName OpenElasticTargetWidgetName = TEXT("PopupCard");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Popup|Animation")
	FVector2D OpenElasticPivot = FVector2D(0.5f, 0.5f);

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

	UPROPERTY(Transient)
	TObjectPtr<UWidget> OpenElasticAnimatedWidget = nullptr;

	UPROPERTY(Transient)
	bool bOpenElasticScaleAnimationActive = false;

	UPROPERTY(Transient)
	float OpenElasticElapsedTime = 0.0f;

	UPROPERTY(Transient)
	double OpenElasticLastTimestamp = -1.0;

	FTimerHandle OpenElasticScaleTimerHandle;
};
