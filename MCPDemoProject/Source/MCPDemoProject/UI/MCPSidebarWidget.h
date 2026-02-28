#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPSidebarWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UMCPABValuePopupWidget;
class UMCPDummyPopupWidget;
class UMCPMessagePopupWidget;
class UMCPScrollGridPopupWidget;
class UMCPScrollTilePopupWidget;
struct FMCPFormPopupResult;

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

	UFUNCTION()
	void HandleMenu4Clicked();

	UFUNCTION()
	void HandleMenu5Clicked();

	UFUNCTION()
	void HandleMenu6Clicked();

	UFUNCTION()
	void HandleMenu7Clicked();

	UFUNCTION()
	void HandleDummy1Clicked();

	UFUNCTION()
	void HandleDummy2Clicked();

	UFUNCTION()
	void HandleDummy3Clicked();

	UFUNCTION()
	void HandleDummy4Clicked();

	UFUNCTION()
	void HandleDummy5Clicked();

private:
	void ResolveMessagePopupClass();
	UMCPMessagePopupWidget* GetOrCreateMessagePopup();
	void ResolveABValuePopupClass();
	UMCPABValuePopupWidget* GetOrCreateABValuePopup();
	void ResolveScrollGridPopupClass();
	UMCPScrollGridPopupWidget* GetOrCreateScrollGridPopup();
	void ResolveScrollTilePopupClass();
	UMCPScrollTilePopupWidget* GetOrCreateScrollTilePopup();
	void OpenDummyPopupByIndex(int32 PopupIndex);
	void ResolveDummyPopupClass(int32 PopupIndex);
	UMCPDummyPopupWidget* GetOrCreateDummyPopup(int32 PopupIndex);
	void HandleAnyDummyPopupClosed();
	void SetPopupModalInput(bool bEnabled, UUserWidget* FocusWidget = nullptr);

	void HandleMessagePopupClosed();
	void HandleABPopupConfirmed(int32 FinalA, int32 FinalB, int32 FinalC);
	void HandleABPopupCancelled();
	void HandleScrollGridPopupClosed();
	void HandleScrollTilePopupClosed();
	void HandleMenu5ConfirmResult(bool bConfirmed);
	void HandleMenu6FormResult(const FMCPFormPopupResult& Result);
	void HandleMenu7MessageClosed();
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
	TObjectPtr<UButton> MCP_Menu4Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_Menu5Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_Menu6Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_Menu7Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MCP_DummyDivider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_Dummy1Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_Dummy2Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_Dummy3Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_Dummy4Button;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_Dummy5Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_Menu1Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_Menu2Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_Menu3Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_Menu4Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_Menu5Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_Menu6Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_Menu7Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_Dummy1Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_Dummy2Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_Dummy3Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_Dummy4Label;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_Dummy5Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_ABStatusLabel;

	UPROPERTY(EditDefaultsOnly, Category = "Menu2")
	int32 Menu2InitialA = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Menu2")
	int32 Menu2InitialB = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Menu2")
	int32 Menu2InitialC = 0;

	UPROPERTY(Transient)
	int32 DisplayedA = 0;

	UPROPERTY(Transient)
	int32 DisplayedB = 0;

	UPROPERTY(Transient)
	int32 DisplayedC = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPMessagePopupWidget> MessagePopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPMessagePopupWidget> MessagePopupWidgetInstance;

	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPABValuePopupWidget> ABValuePopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPABValuePopupWidget> ABValuePopupWidgetInstance;

	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPScrollGridPopupWidget> ScrollGridPopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPScrollGridPopupWidget> ScrollGridPopupWidgetInstance;

	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPScrollTilePopupWidget> ScrollTilePopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPScrollTilePopupWidget> ScrollTilePopupWidgetInstance;

	UPROPERTY(Transient)
	TArray<TSubclassOf<UMCPDummyPopupWidget>> DummyPopupWidgetClasses;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMCPDummyPopupWidget>> DummyPopupWidgetInstances;
};
