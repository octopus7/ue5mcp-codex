#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPSidebarWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UMCPABValuePopupWidget;
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
	void ResolveABValuePopupClass();
	UMCPABValuePopupWidget* GetOrCreateABValuePopup();
	void SetPopupModalInput(bool bEnabled, UUserWidget* FocusWidget = nullptr);

	void HandleMessagePopupClosed();
	void HandleABPopupConfirmed(int32 FinalA, int32 FinalB);
	void HandleABPopupCancelled();
	void RefreshABValueDisplay();

	void ShowDebugMessage(const FString& Message, const FColor& Color) const;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> MCP_SidebarPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_Menu1Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_Menu2Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_Menu3Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_Menu1Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_Menu2Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_Menu3Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_ABStatusLabel;

	UPROPERTY(EditDefaultsOnly, Category = "Menu2")
	int32 Menu2InitialA = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Menu2")
	int32 Menu2InitialB = 0;

	UPROPERTY(Transient)
	int32 DisplayedA = 0;

	UPROPERTY(Transient)
	int32 DisplayedB = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPMessagePopupWidget> MessagePopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPMessagePopupWidget> MessagePopupWidgetInstance;

	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPABValuePopupWidget> ABValuePopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPABValuePopupWidget> ABValuePopupWidgetInstance;
};
