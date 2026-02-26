#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPSidebarWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UMCPMessagePopupWidget;

UCLASS()
class MCPDEMOPROJECT_API UMCPSidebarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	UFUNCTION()
	void HandleMenu1Clicked();

	UFUNCTION()
	void HandleMenu2Clicked();

	UFUNCTION()
	void HandleMenu3Clicked();

private:
	void ResolveMessagePopupClass();
	UMCPMessagePopupWidget* GetOrCreateMessagePopup();
	void SetPopupModalInput(bool bEnabled);

	void HandleMessagePopupClosed();

	void ResolveWidgets();
	void ApplyVisualStyle();
	void ApplyLabels();
	void ShowDebugMessage(const FString& Message, const FColor& Color) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> SidebarPanel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Menu1Button;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Menu2Button;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Menu3Button;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Menu1Label;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Menu2Label;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Menu3Label;

	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPMessagePopupWidget> MessagePopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPMessagePopupWidget> MessagePopupWidgetInstance;
};
