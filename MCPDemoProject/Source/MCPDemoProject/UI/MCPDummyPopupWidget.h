#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPDummyPopupWidget.generated.h"

class UBackgroundBlur;
class UBorder;
class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FOnDummyPopupClosed);

UCLASS()
class MCPDEMOPROJECT_API UMCPDummyPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void OpenPopup(const FText& InTitle, const FText& InMessage);

	FOnDummyPopupClosed OnClosed;

protected:
	UFUNCTION()
	void HandleCloseClicked();

private:
	void BuildFallbackWidgetTreeIfNeeded();
	void ApplyDefaultTexts();

private:
	bool bCloseBroadcasted = false;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MCP_DimBlocker;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBackgroundBlur> MCP_BlurPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MCP_MessageBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MCP_IdentityTintPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_MessageText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_CloseLabel;
};
