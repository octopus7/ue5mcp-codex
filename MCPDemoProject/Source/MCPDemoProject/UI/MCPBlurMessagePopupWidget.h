#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPBlurMessagePopupWidget.generated.h"

class UBackgroundBlur;
class UBorder;
class UButton;
class UTextBlock;

DECLARE_DELEGATE(FMCPMessagePopupClosedCallback);
DECLARE_MULTICAST_DELEGATE(FOnBlurMessagePopupClosed);

struct FMCPMessagePopupRequest
{
	FText Title;
	FText Message;
	FText OkLabel;
	FMCPMessagePopupClosedCallback Callback;
};

UCLASS()
class MCPDEMOPROJECT_API UMCPBlurMessagePopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void OpenPopup(const FMCPMessagePopupRequest& InRequest);

	FOnBlurMessagePopupClosed OnClosed;

protected:
	UFUNCTION()
	void HandleOkClicked();

private:
	void BuildFallbackWidgetTreeIfNeeded();
	void ApplyDefaultTexts();

private:
	bool bCloseBroadcasted = false;
	FMCPMessagePopupClosedCallback ActiveCloseCallback;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MCP_DimBlocker;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBackgroundBlur> MCP_BlurPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MCP_MessageBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_MessageText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_OkButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_OkLabel;
};
