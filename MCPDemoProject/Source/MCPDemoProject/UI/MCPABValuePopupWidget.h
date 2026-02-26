#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPABValuePopupWidget.generated.h"

class UBorder;
class UButton;
class USlider;
class USpacer;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnABPopupConfirmed, int32, int32, int32);
DECLARE_MULTICAST_DELEGATE(FOnABPopupCancelled);

UCLASS()
class MCPDEMOPROJECT_API UMCPABValuePopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void OpenPopup(int32 InInitialA, int32 InInitialB, int32 InInitialC);

	FOnABPopupConfirmed OnABPopupConfirmed;
	FOnABPopupCancelled OnABPopupCancelled;

protected:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleADecClicked();

	UFUNCTION()
	void HandleAIncClicked();

	UFUNCTION()
	void HandleBDecClicked();

	UFUNCTION()
	void HandleBIncClicked();

	UFUNCTION()
	void HandleCSliderValueChanged(float InValue);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

private:
	void ApplyDefaultTexts();
	void RefreshValueTexts();
	int32 ClampToRange(int32 InValue) const;

private:
	bool bCloseBroadcasted = false;

	int32 CurrentA = 0;

	int32 CurrentB = 0;

	int32 CurrentC = 0;

	UPROPERTY(EditDefaultsOnly, Category = "AB")
	int32 MinValue = 0;

	UPROPERTY(EditDefaultsOnly, Category = "AB")
	int32 MaxValue = 100;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> MCP_DimBlocker;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> MCP_PopupPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_CloseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_CloseLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_ALabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_ADecButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_ADecLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_AValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_AIncButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_AIncLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_BLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_BDecButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_BDecLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_BValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_BIncButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_BIncLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_CLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> MCP_CSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_CValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USpacer> MCP_BottomSpacer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_ConfirmLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_CancelButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_CancelLabel;
};
