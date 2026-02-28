#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPFormPopupWidget.generated.h"

class UBorder;
class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;

struct FMCPFormPopupResult
{
	bool bSubmitted = false;
	FText NameValue;
	bool bChecked = false;
	FString SelectedOption;
};

DECLARE_DELEGATE_OneParam(FMCPFormPopupResultCallback, const FMCPFormPopupResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFormPopupSubmitted, const FMCPFormPopupResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFormPopupCancelled, const FMCPFormPopupResult&);

struct FMCPFormPopupRequest
{
	FText Title;
	FText Message;
	FText NameHint;
	FText CheckLabel;
	TArray<FString> ComboOptions;
	int32 DefaultComboIndex = 0;
	FText SubmitLabel;
	FText CancelLabel;
	FMCPFormPopupResultCallback Callback;
};

UCLASS()
class MCPDEMOPROJECT_API UMCPFormPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void OpenPopup(const FMCPFormPopupRequest& Request);

	FOnFormPopupSubmitted OnSubmitted;
	FOnFormPopupCancelled OnCancelled;

protected:
	UFUNCTION()
	void HandleSubmitClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleNameTextChanged(const FText& NewText);

private:
	void BuildFallbackWidgetTreeIfNeeded();
	void ApplyDefaultTexts();
	void PopulateComboOptions(const FMCPFormPopupRequest& Request);
	void UpdateSubmitButtonEnabledState();
	void CollectCurrentResult(FMCPFormPopupResult& OutResult, bool bSubmitted) const;

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
	TObjectPtr<UEditableTextBox> MCP_NameInput;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> MCP_CheckBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_CheckLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> MCP_ComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_SubmitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_SubmitLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MCP_CancelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MCP_CancelLabel;
};

