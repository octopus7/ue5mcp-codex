#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPConfirmPopupWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FMCPConfirmPopupResultCallback, bool);
DECLARE_MULTICAST_DELEGATE(FOnConfirmPopupConfirmed);
DECLARE_MULTICAST_DELEGATE(FOnConfirmPopupCancelled);

struct FMCPConfirmPopupRequest
{
	FText Title;
	FText Message;
	FText ConfirmLabel;
	FText CancelLabel;
	FMCPConfirmPopupResultCallback Callback;
};

UCLASS()
class MCPDEMOPROJECT_API UMCPConfirmPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void OpenPopup(const FMCPConfirmPopupRequest& InRequest);

	FOnConfirmPopupConfirmed OnConfirmed;
	FOnConfirmPopupCancelled OnCancelled;

protected:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

private:
	void BuildFallbackWidgetTreeIfNeeded();
	void ApplyDefaultTexts();

private:
	bool bCloseBroadcasted = false;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MCP_DimBlocker;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MCP_PopupPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_MessageText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_ConfirmButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_ConfirmLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_CancelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_CancelLabel;
};
