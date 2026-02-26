#include "MCPScrollTilePopupWidget.h"

#include "MCPScrollTileItemObject.h"
#include "MCPScrollTileItemWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ListViewBase.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Styling/SlateBrush.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "Widgets/Views/STableViewBase.h"

void UMCPScrollTilePopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ResolveItemWidgetClass();
	EnsureTileViewEntryClass(MCP_ItemTileView);
}

#if WITH_EDITOR
void UMCPScrollTilePopupWidget::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);

	UWidget* FoundWidget = BlueprintWidgetTree.FindWidget(FName(TEXT("MCP_ItemTileView")));
	UTileView* TileView = Cast<UTileView>(FoundWidget);
	if (TileView == nullptr || TileView->GetEntryWidgetClass() != nullptr)
	{
		return;
	}

	const TSubclassOf<UUserWidget> EntryClass = ResolveTileEntryClass();
	if (EntryClass == nullptr)
	{
		return;
	}

	if (FClassProperty* EntryClassProperty = FindFProperty<FClassProperty>(UListViewBase::StaticClass(), TEXT("EntryWidgetClass")))
	{
		EntryClassProperty->SetPropertyValue_InContainer(TileView, EntryClass.Get());
	}
}
#endif

void UMCPScrollTilePopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFallbackWidgetTreeIfNeeded();
	ResolveItemWidgetClass();
	ResolveIconTextures();
	EnsureTileViewEntryClass(MCP_ItemTileView);
	ApplyDefaultTexts();
	RefreshCountText();
	bCloseBroadcasted = false;

	if (MCP_ItemTileView != nullptr)
	{
		MCP_ItemTileView->SetEntryWidth(172.0f);
		MCP_ItemTileView->SetEntryHeight(172.0f);
		MCP_ItemTileView->SetSelectionMode(ESelectionMode::None);
		MCP_ItemTileView->OnGetEntryClassForItem().BindUObject(this, &UMCPScrollTilePopupWidget::GetEntryClassForItem);
	}

	if (MCP_AddItemsButton != nullptr)
	{
		MCP_AddItemsButton->OnClicked.RemoveDynamic(this, &UMCPScrollTilePopupWidget::HandleAddItemsClicked);
		MCP_AddItemsButton->OnClicked.AddDynamic(this, &UMCPScrollTilePopupWidget::HandleAddItemsClicked);
	}

	if (MCP_CloseButton != nullptr)
	{
		MCP_CloseButton->OnClicked.RemoveDynamic(this, &UMCPScrollTilePopupWidget::HandleCloseClicked);
		MCP_CloseButton->OnClicked.AddDynamic(this, &UMCPScrollTilePopupWidget::HandleCloseClicked);
	}
}

FReply UMCPScrollTilePopupWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && MCP_ItemTileView != nullptr)
	{
		const FGeometry TileGeometry = MCP_ItemTileView->GetCachedGeometry();
		if (TileGeometry.IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
		{
			bDragScrolling = true;
			DragStartMouseY = InMouseEvent.GetScreenSpacePosition().Y;
			DragStartScrollOffset = MCP_ItemTileView->GetScrollOffset();

			if (const TSharedPtr<SWidget> CachedWidget = GetCachedWidget())
			{
				return FReply::Handled().CaptureMouse(CachedWidget.ToSharedRef());
			}
			return FReply::Handled();
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMCPScrollTilePopupWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragScrolling && MCP_ItemTileView != nullptr)
	{
		if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			bDragScrolling = false;
			return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
		}

		const float DeltaY = InMouseEvent.GetScreenSpacePosition().Y - DragStartMouseY;
		const float TargetOffset = FMath::Max(0.0f, DragStartScrollOffset - DeltaY);
		MCP_ItemTileView->SetScrollOffset(TargetOffset);
		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UMCPScrollTilePopupWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragScrolling && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragScrolling = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UMCPScrollTilePopupWidget::OpenPopup()
{
	ResetToInitialItems();
	bCloseBroadcasted = false;
	SetVisibility(ESlateVisibility::Visible);
	SetIsEnabled(true);
}

void UMCPScrollTilePopupWidget::HandleAddItemsClicked()
{
	AppendItems(AddBatchCount);
}

void UMCPScrollTilePopupWidget::HandleCloseClicked()
{
	if (bCloseBroadcasted)
	{
		return;
	}

	bCloseBroadcasted = true;
	OnScrollTilePopupClosed.Broadcast();
	RemoveFromParent();
}

void UMCPScrollTilePopupWidget::EnsureTileViewEntryClass(UTileView* TileView)
{
	if (TileView == nullptr || TileView->GetEntryWidgetClass() != nullptr)
	{
		return;
	}

	const TSubclassOf<UUserWidget> EntryClass = ResolveTileEntryClass();
	if (EntryClass == nullptr)
	{
		return;
	}

	if (FClassProperty* EntryClassProperty = FindFProperty<FClassProperty>(UListViewBase::StaticClass(), TEXT("EntryWidgetClass")))
	{
		EntryClassProperty->SetPropertyValue_InContainer(TileView, EntryClass.Get());
	}
}

TSubclassOf<UUserWidget> UMCPScrollTilePopupWidget::ResolveTileEntryClass() const
{
	if (ItemWidgetClass != nullptr)
	{
		return ItemWidgetClass.Get();
	}

	static const TCHAR* EntryClassPath = TEXT("/Game/UI/Widget/WBP_MCPScrollTileItem.WBP_MCPScrollTileItem_C");
	UClass* LoadedClass = LoadClass<UUserWidget>(nullptr, EntryClassPath);
	if (LoadedClass == nullptr)
	{
		const FSoftClassPath SoftClassPath(EntryClassPath);
		LoadedClass = SoftClassPath.TryLoadClass<UUserWidget>();
	}

	return LoadedClass;
}

void UMCPScrollTilePopupWidget::BuildFallbackWidgetTreeIfNeeded()
{
	if (MCP_DimBlocker != nullptr &&
		MCP_PopupPanel != nullptr &&
		MCP_TitleText != nullptr &&
		MCP_AddItemsButton != nullptr &&
		MCP_AddItemsLabel != nullptr &&
		MCP_CloseButton != nullptr &&
		MCP_CloseLabel != nullptr &&
		MCP_ItemTileView != nullptr &&
		MCP_CountText != nullptr)
	{
		return;
	}

	if (WidgetTree == nullptr)
	{
		return;
	}

	if (WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MCP_RootCanvas"));
	if (RootCanvas == nullptr)
	{
		return;
	}
	WidgetTree->RootWidget = RootCanvas;

	MCP_DimBlocker = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MCP_DimBlocker"));
	if (MCP_DimBlocker != nullptr)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::Image;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.03f, 0.03f, 0.03f, 0.72f));
		MCP_DimBlocker->SetBrush(DimBrush);
		RootCanvas->AddChild(MCP_DimBlocker);

		if (UCanvasPanelSlot* DimSlot = Cast<UCanvasPanelSlot>(MCP_DimBlocker->Slot))
		{
			DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			DimSlot->SetOffsets(FMargin(0.0f));
			DimSlot->SetZOrder(0);
		}
	}

	MCP_PopupPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MCP_PopupPanel"));
	if (MCP_PopupPanel == nullptr)
	{
		return;
	}

	{
		FSlateBrush PanelBrush;
		PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		PanelBrush.TintColor = FSlateColor(FLinearColor(0.11f, 0.12f, 0.14f, 0.98f));
		PanelBrush.OutlineSettings = FSlateBrushOutlineSettings(
			FVector4(14.0f, 14.0f, 14.0f, 14.0f),
			FSlateColor(FLinearColor(0.22f, 0.24f, 0.27f, 1.0f)),
			1.0f);
		MCP_PopupPanel->SetBrush(PanelBrush);
		MCP_PopupPanel->SetPadding(FMargin(24.0f));
	}

	RootCanvas->AddChild(MCP_PopupPanel);
	if (UCanvasPanelSlot* PopupSlot = Cast<UCanvasPanelSlot>(MCP_PopupPanel->Slot))
	{
		PopupSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PopupSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PopupSlot->SetSize(FVector2D(980.0f, 680.0f));
		PopupSlot->SetPosition(FVector2D::ZeroVector);
		PopupSlot->SetZOrder(1);
	}

	UVerticalBox* PopupVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MCP_PopupVerticalBox"));
	if (PopupVerticalBox == nullptr)
	{
		return;
	}
	MCP_PopupPanel->SetContent(PopupVerticalBox);

	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MCP_HeaderRow"));
	if (HeaderRow != nullptr)
	{
		PopupVerticalBox->AddChild(HeaderRow);
		if (UVerticalBoxSlot* HeaderSlot = Cast<UVerticalBoxSlot>(HeaderRow->Slot))
		{
			HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}

		MCP_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_TitleText"));
		if (MCP_TitleText != nullptr)
		{
			MCP_TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.93f, 0.96f, 1.0f)));
			HeaderRow->AddChildToHorizontalBox(MCP_TitleText);
			if (UHorizontalBoxSlot* TitleSlot = Cast<UHorizontalBoxSlot>(MCP_TitleText->Slot))
			{
				FSlateChildSize FillSize;
				FillSize.SizeRule = ESlateSizeRule::Fill;
				FillSize.Value = 1.0f;
				TitleSlot->SetSize(FillSize);
				TitleSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		MCP_CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MCP_CloseButton"));
		if (MCP_CloseButton != nullptr)
		{
			HeaderRow->AddChildToHorizontalBox(MCP_CloseButton);
			if (UHorizontalBoxSlot* CloseButtonSlot = Cast<UHorizontalBoxSlot>(MCP_CloseButton->Slot))
			{
				CloseButtonSlot->SetHorizontalAlignment(HAlign_Right);
				CloseButtonSlot->SetVerticalAlignment(VAlign_Center);
			}

			MCP_CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_CloseLabel"));
			if (MCP_CloseLabel != nullptr)
			{
				MCP_CloseLabel->SetJustification(ETextJustify::Center);
				MCP_CloseLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				MCP_CloseButton->SetContent(MCP_CloseLabel);
			}
		}
	}

	MCP_ItemTileView = WidgetTree->ConstructWidget<UTileView>(UTileView::StaticClass(), TEXT("MCP_ItemTileView"));
	if (MCP_ItemTileView != nullptr)
	{
		PopupVerticalBox->AddChild(MCP_ItemTileView);
		if (UVerticalBoxSlot* TileSlot = Cast<UVerticalBoxSlot>(MCP_ItemTileView->Slot))
		{
			FSlateChildSize FillSize;
			FillSize.SizeRule = ESlateSizeRule::Fill;
			FillSize.Value = 1.0f;
			TileSlot->SetSize(FillSize);
			TileSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}
	}

	UHorizontalBox* FooterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MCP_FooterRow"));
	if (FooterRow != nullptr)
	{
		PopupVerticalBox->AddChild(FooterRow);
		if (UVerticalBoxSlot* FooterSlot = Cast<UVerticalBoxSlot>(FooterRow->Slot))
		{
			FooterSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}

		MCP_CountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_CountText"));
		if (MCP_CountText != nullptr)
		{
			MCP_CountText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.85f, 0.90f, 1.0f)));
			FooterRow->AddChildToHorizontalBox(MCP_CountText);
			if (UHorizontalBoxSlot* CountSlot = Cast<UHorizontalBoxSlot>(MCP_CountText->Slot))
			{
				FSlateChildSize FillSize;
				FillSize.SizeRule = ESlateSizeRule::Fill;
				FillSize.Value = 1.0f;
				CountSlot->SetSize(FillSize);
				CountSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		MCP_AddItemsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MCP_AddItemsButton"));
		if (MCP_AddItemsButton != nullptr)
		{
			FooterRow->AddChildToHorizontalBox(MCP_AddItemsButton);
			if (UHorizontalBoxSlot* AddButtonSlot = Cast<UHorizontalBoxSlot>(MCP_AddItemsButton->Slot))
			{
				AddButtonSlot->SetHorizontalAlignment(HAlign_Right);
				AddButtonSlot->SetVerticalAlignment(VAlign_Center);
			}

			MCP_AddItemsLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_AddItemsLabel"));
			if (MCP_AddItemsLabel != nullptr)
			{
				MCP_AddItemsLabel->SetJustification(ETextJustify::Center);
				MCP_AddItemsLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				MCP_AddItemsButton->SetContent(MCP_AddItemsLabel);
			}
		}
	}
}

void UMCPScrollTilePopupWidget::ApplyDefaultTexts()
{
	if (MCP_TitleText != nullptr && MCP_TitleText->GetText().IsEmpty())
	{
		MCP_TitleText->SetText(FText::FromString(TEXT("Scroll TileView Popup")));
	}

	if (MCP_CloseLabel != nullptr && MCP_CloseLabel->GetText().IsEmpty())
	{
		MCP_CloseLabel->SetText(FText::FromString(TEXT("X")));
	}

	if (MCP_AddItemsLabel != nullptr && MCP_AddItemsLabel->GetText().IsEmpty())
	{
		MCP_AddItemsLabel->SetText(FText::FromString(TEXT("Add 25")));
	}
}

void UMCPScrollTilePopupWidget::ResolveItemWidgetClass()
{
	if (ItemWidgetClass != nullptr)
	{
		return;
	}

	static const TCHAR* ItemClassPath = TEXT("/Game/UI/Widget/WBP_MCPScrollTileItem.WBP_MCPScrollTileItem_C");
	UClass* LoadedClass = LoadClass<UMCPScrollTileItemWidget>(nullptr, ItemClassPath);
	if (LoadedClass == nullptr)
	{
		const FSoftClassPath SoftClassPath(ItemClassPath);
		LoadedClass = SoftClassPath.TryLoadClass<UMCPScrollTileItemWidget>();
	}

	if (LoadedClass != nullptr)
	{
		ItemWidgetClass = LoadedClass;
	}
	else
	{
		ItemWidgetClass = UMCPScrollTileItemWidget::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("[MCPScrollTilePopup] Item widget class not found at %s. Falling back to native class."), ItemClassPath);
	}
}

void UMCPScrollTilePopupWidget::ResolveIconTextures()
{
	if (!IconTextures.IsEmpty())
	{
		return;
	}

	const FString IconsDir = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UI"), TEXT("Icons"));
	const FString IconsSearchPattern = FPaths::Combine(IconsDir, TEXT("*.uasset"));
	TArray<FString> IconAssetFiles;
	IFileManager::Get().FindFiles(IconAssetFiles, *IconsSearchPattern, true, false);

	for (const FString& IconAssetFile : IconAssetFiles)
	{
		const FString AssetName = FPaths::GetBaseFilename(IconAssetFile);
		const FString AssetPath = FString::Printf(TEXT("/Game/UI/Icons/%s.%s"), *AssetName, *AssetName);
		if (UTexture2D* IconTexture = LoadObject<UTexture2D>(nullptr, *AssetPath))
		{
			IconTextures.Add(IconTexture);
		}
	}

	if (IconTextures.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MCPScrollTilePopup] No textures found in /Game/UI/Icons."));
	}
}

TSubclassOf<UUserWidget> UMCPScrollTilePopupWidget::GetEntryClassForItem(UObject* ItemObject)
{
	(void)ItemObject;
	if (ItemWidgetClass != nullptr)
	{
		return ItemWidgetClass.Get();
	}

	return UMCPScrollTileItemWidget::StaticClass();
}

void UMCPScrollTilePopupWidget::ResetToInitialItems()
{
	if (MCP_ItemTileView == nullptr)
	{
		return;
	}

	MCP_ItemTileView->ClearListItems();
	ItemObjects.Reset();
	ItemCount = 0;
	AppendItems(InitialItemCount);
	MCP_ItemTileView->ScrollToTop();
}

void UMCPScrollTilePopupWidget::AppendItems(int32 Count)
{
	if (Count <= 0 || MCP_ItemTileView == nullptr)
	{
		return;
	}

	ResolveItemWidgetClass();
	if (ItemWidgetClass == nullptr)
	{
		return;
	}

	for (int32 Offset = 0; Offset < Count; ++Offset)
	{
		const int32 ItemIndex = ItemCount + 1;
		UMCPScrollTileItemObject* ItemObject = CreateRandomItemData(ItemIndex);
		if (ItemObject == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MCPScrollTilePopup] Failed to create item data for index %d."), ItemIndex);
			break;
		}

		ItemObjects.Add(ItemObject);
		MCP_ItemTileView->AddItem(ItemObject);
		ItemCount = ItemIndex;
	}

	RefreshCountText();
}

void UMCPScrollTilePopupWidget::RefreshCountText()
{
	if (MCP_CountText != nullptr)
	{
		MCP_CountText->SetText(FText::FromString(FString::Printf(TEXT("Items: %d"), ItemCount)));
	}
}

UMCPScrollTileItemObject* UMCPScrollTilePopupWidget::CreateRandomItemData(int32 ItemIndex)
{
	const float Hue01 = FMath::FRand();
	const float Saturation = FMath::FRandRange(0.45f, 0.85f);
	const float Value = FMath::FRandRange(0.55f, 0.90f);

	const uint8 Hue = static_cast<uint8>(FMath::RoundToInt(Hue01 * 255.0f));
	const uint8 Sat = static_cast<uint8>(FMath::RoundToInt(Saturation * 255.0f));
	const uint8 Val = static_cast<uint8>(FMath::RoundToInt(Value * 255.0f));

	UMCPScrollTileItemObject* Data = NewObject<UMCPScrollTileItemObject>(this);
	if (Data == nullptr)
	{
		return nullptr;
	}

	Data->BackgroundColor = FLinearColor::MakeFromHSV8(Hue, Sat, Val);
	Data->Name = FText::FromString(FString::Printf(TEXT("Item %d"), ItemIndex));
	ResolveIconTextures();
	if (!IconTextures.IsEmpty())
	{
		const int32 IconIndex = FMath::RandRange(0, IconTextures.Num() - 1);
		Data->IconTexture = IconTextures[IconIndex];
	}
	return Data;
}
