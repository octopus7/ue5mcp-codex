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
	void ResolveWidgets();
	void ApplyVisualStyle();
	void ApplyDefaultTexts();

private:
	bool bCloseBroadcasted = false;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DimBlocker;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PopupPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> OkButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OkLabel;
};
