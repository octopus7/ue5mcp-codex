#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPMessagePopupWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FOnPopupClosed);

UCLASS()
class MCPDEMOPROJECT_API UMCPMessagePopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void OpenPopup(const FText& InTitle, const FText& InMessage);

	FOnPopupClosed OnPopupClosed;

protected:
	UFUNCTION()
	void HandleOkClicked();

private:
	void ApplyDefaultTexts();

private:
	bool bCloseBroadcasted = false;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> MCP_DimBlocker;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> MCP_PopupPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_MessageText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_OkButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_OkLabel;
};
