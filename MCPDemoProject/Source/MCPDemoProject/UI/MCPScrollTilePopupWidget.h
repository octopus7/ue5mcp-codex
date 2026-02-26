#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "MCPScrollTilePopupWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UTexture2D;
class UTileView;
class UMCPScrollTileItemObject;
class UMCPScrollTileItemWidget;
class UUserWidget;
class UWidgetTree;
struct FGeometry;
class IWidgetCompilerLog;
struct FPointerEvent;

DECLARE_MULTICAST_DELEGATE(FOnScrollTilePopupClosed);

UCLASS()
class MCPDEMOPROJECT_API UMCPScrollTilePopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const override;
#endif

	void OpenPopup();

	FOnScrollTilePopupClosed OnScrollTilePopupClosed;

protected:
	UFUNCTION()
	void HandleAddItemsClicked();

	UFUNCTION()
	void HandleCloseClicked();

private:
	void EnsureTileViewEntryClass(UTileView* TileView);
	TSubclassOf<UUserWidget> ResolveTileEntryClass() const;
	void BuildFallbackWidgetTreeIfNeeded();
	void ApplyDefaultTexts();
	void ResolveItemWidgetClass();
	void ResolveIconTextures();
	TSubclassOf<UUserWidget> GetEntryClassForItem(UObject* ItemObject);
	void ResetToInitialItems();
	void AppendItems(int32 Count);
	void RefreshCountText();
	UMCPScrollTileItemObject* CreateRandomItemData(int32 ItemIndex);

private:
	static constexpr int32 InitialItemCount = 50;
	static constexpr int32 AddBatchCount = 25;

	bool bCloseBroadcasted = false;
	bool bDragScrolling = false;
	float DragStartMouseY = 0.0f;
	float DragStartScrollOffset = 0.0f;
	int32 ItemCount = 0;

	UPROPERTY(EditDefaultsOnly, Category = "ScrollTile")
	TSubclassOf<UMCPScrollTileItemWidget> ItemWidgetClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMCPScrollTileItemObject>> ItemObjects;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> IconTextures;

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
	TObjectPtr<UTileView> MCP_ItemTileView;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_CountText;
};
