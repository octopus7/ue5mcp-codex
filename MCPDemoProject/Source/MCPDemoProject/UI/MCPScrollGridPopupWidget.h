#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "MCPScrollGridItemWidget.h"
#include "MCPScrollGridPopupWidget.generated.h"

class UBorder;
class UButton;
class UScrollBox;
class UTextBlock;
class UUniformGridPanel;
class UMCPScrollGridItemWidget;
struct FGeometry;
struct FPointerEvent;

DECLARE_MULTICAST_DELEGATE(FOnScrollGridPopupClosed);

UCLASS()
class MCPDEMOPROJECT_API UMCPScrollGridPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	void OpenPopup();

	FOnScrollGridPopupClosed OnScrollGridPopupClosed;

protected:
	UFUNCTION()
	void HandleAddItemsClicked();

	UFUNCTION()
	void HandleCloseClicked();

private:
	void BuildFallbackWidgetTreeIfNeeded();
	void ApplyDefaultTexts();
	void ResolveItemWidgetClass();
	void ResetToInitialItems();
	void AppendItems(int32 Count);
	void RefreshCountText();
	FMCPScrollGridItemData CreateRandomItemData(int32 ItemIndex) const;

private:
	static constexpr int32 GridColumns = 5;
	static constexpr int32 InitialItemCount = 50;
	static constexpr int32 AddBatchCount = 25;

	bool bCloseBroadcasted = false;
	bool bDragScrolling = false;
	float DragStartMouseY = 0.0f;
	float DragStartScrollOffset = 0.0f;
	int32 ItemCount = 0;

	UPROPERTY(EditDefaultsOnly, Category = "ScrollGrid")
	TSubclassOf<UMCPScrollGridItemWidget> ItemWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> MCP_DimBlocker;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> MCP_PopupPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_AddItemsButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_AddItemsLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MCP_CloseButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_CloseLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> MCP_ItemScrollBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> MCP_ItemGrid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_CountText;
};
