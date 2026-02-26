#include "MCPScrollGridPopupWidget.h"

#include "MCPScrollGridItemWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Styling/SlateBrush.h"
#include "UObject/SoftObjectPath.h"

void UMCPScrollGridPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFallbackWidgetTreeIfNeeded();
	ResolveItemWidgetClass();
	ApplyDefaultTexts();

	if (MCP_ItemGrid != nullptr)
	{
		MCP_ItemGrid->SetMinDesiredSlotWidth(172.0f);
		MCP_ItemGrid->SetMinDesiredSlotHeight(100.0f);
		MCP_ItemGrid->SetSlotPadding(FMargin(6.0f, 12.0f));
	}

	RefreshCountText();
	bCloseBroadcasted = false;

	if (MCP_AddItemsButton != nullptr)
	{
		MCP_AddItemsButton->OnClicked.RemoveDynamic(this, &UMCPScrollGridPopupWidget::HandleAddItemsClicked);
		MCP_AddItemsButton->OnClicked.AddDynamic(this, &UMCPScrollGridPopupWidget::HandleAddItemsClicked);
	}

	if (MCP_CloseButton != nullptr)
	{
		MCP_CloseButton->OnClicked.RemoveDynamic(this, &UMCPScrollGridPopupWidget::HandleCloseClicked);
		MCP_CloseButton->OnClicked.AddDynamic(this, &UMCPScrollGridPopupWidget::HandleCloseClicked);
	}
}

FReply UMCPScrollGridPopupWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && MCP_ItemScrollBox != nullptr)
	{
		const FGeometry ScrollGeometry = MCP_ItemScrollBox->GetCachedGeometry();
		if (ScrollGeometry.IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
		{
			bDragScrolling = true;
			DragStartMouseY = InMouseEvent.GetScreenSpacePosition().Y;
			DragStartScrollOffset = MCP_ItemScrollBox->GetScrollOffset();

			if (const TSharedPtr<SWidget> CachedWidget = GetCachedWidget())
			{
				return FReply::Handled().CaptureMouse(CachedWidget.ToSharedRef());
			}
			return FReply::Handled();
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMCPScrollGridPopupWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragScrolling && MCP_ItemScrollBox != nullptr)
	{
		if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			bDragScrolling = false;
			return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
		}

		const float DeltaY = InMouseEvent.GetScreenSpacePosition().Y - DragStartMouseY;
		const float TargetOffset = FMath::Max(0.0f, DragStartScrollOffset - DeltaY);
		MCP_ItemScrollBox->SetScrollOffset(TargetOffset);
		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UMCPScrollGridPopupWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragScrolling && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragScrolling = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UMCPScrollGridPopupWidget::OpenPopup()
{
	ResetToInitialItems();
	bCloseBroadcasted = false;
	SetVisibility(ESlateVisibility::Visible);
	SetIsEnabled(true);
}

void UMCPScrollGridPopupWidget::HandleAddItemsClicked()
{
	AppendItems(AddBatchCount);
}

void UMCPScrollGridPopupWidget::HandleCloseClicked()
{
	if (bCloseBroadcasted)
	{
		return;
	}

	bCloseBroadcasted = true;
	OnScrollGridPopupClosed.Broadcast();
	RemoveFromParent();
}

void UMCPScrollGridPopupWidget::BuildFallbackWidgetTreeIfNeeded()
{
	if (MCP_DimBlocker != nullptr &&
		MCP_PopupPanel != nullptr &&
		MCP_TitleText != nullptr &&
		MCP_AddItemsButton != nullptr &&
		MCP_AddItemsLabel != nullptr &&
		MCP_CloseButton != nullptr &&
		MCP_CloseLabel != nullptr &&
		MCP_ItemScrollBox != nullptr &&
		MCP_ItemGrid != nullptr &&
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

	MCP_ItemScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MCP_ItemScrollBox"));
	if (MCP_ItemScrollBox != nullptr)
	{
		PopupVerticalBox->AddChild(MCP_ItemScrollBox);
		if (UVerticalBoxSlot* ScrollSlot = Cast<UVerticalBoxSlot>(MCP_ItemScrollBox->Slot))
		{
			FSlateChildSize FillSize;
			FillSize.SizeRule = ESlateSizeRule::Fill;
			FillSize.Value = 1.0f;
			ScrollSlot->SetSize(FillSize);
			ScrollSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}

		MCP_ItemGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("MCP_ItemGrid"));
		if (MCP_ItemGrid != nullptr)
		{
			MCP_ItemGrid->SetMinDesiredSlotWidth(172.0f);
			MCP_ItemGrid->SetMinDesiredSlotHeight(100.0f);
			MCP_ItemGrid->SetSlotPadding(FMargin(6.0f, 12.0f));
			MCP_ItemScrollBox->AddChild(MCP_ItemGrid);
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

void UMCPScrollGridPopupWidget::ApplyDefaultTexts()
{
	if (MCP_TitleText != nullptr && MCP_TitleText->GetText().IsEmpty())
	{
		MCP_TitleText->SetText(FText::FromString(TEXT("Scroll Grid Box Popup")));
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

void UMCPScrollGridPopupWidget::ResolveItemWidgetClass()
{
	if (ItemWidgetClass != nullptr)
	{
		return;
	}

	static const TCHAR* ItemClassPath = TEXT("/Game/UI/Widget/WBP_MCPScrollGridItem.WBP_MCPScrollGridItem_C");
	UClass* LoadedClass = LoadClass<UMCPScrollGridItemWidget>(nullptr, ItemClassPath);
	if (LoadedClass == nullptr)
	{
		const FSoftClassPath SoftClassPath(ItemClassPath);
		LoadedClass = SoftClassPath.TryLoadClass<UMCPScrollGridItemWidget>();
	}

	if (LoadedClass != nullptr)
	{
		ItemWidgetClass = LoadedClass;
	}
	else
	{
		ItemWidgetClass = UMCPScrollGridItemWidget::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("[MCPScrollGridPopup] Item widget class not found at %s. Falling back to native class."), ItemClassPath);
	}
}

void UMCPScrollGridPopupWidget::ResetToInitialItems()
{
	if (MCP_ItemGrid == nullptr)
	{
		return;
	}

	MCP_ItemGrid->ClearChildren();
	ItemCount = 0;
	AppendItems(InitialItemCount);

	if (MCP_ItemScrollBox != nullptr)
	{
		MCP_ItemScrollBox->ScrollToStart();
	}
}

void UMCPScrollGridPopupWidget::AppendItems(int32 Count)
{
	if (Count <= 0 || MCP_ItemGrid == nullptr)
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
		UMCPScrollGridItemWidget* ItemWidget = CreateWidget<UMCPScrollGridItemWidget>(this, ItemWidgetClass);
		if (ItemWidget == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MCPScrollGridPopup] Failed to create item widget for index %d."), ItemIndex);
			break;
		}

		ItemWidget->ApplyItemData(CreateRandomItemData(ItemIndex));

		const int32 Row = (ItemIndex - 1) / GridColumns;
		const int32 Column = (ItemIndex - 1) % GridColumns;
		UUniformGridSlot* GridSlot = MCP_ItemGrid->AddChildToUniformGrid(ItemWidget, Row, Column);

		if (GridSlot != nullptr)
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}

		ItemCount = ItemIndex;
	}

	RefreshCountText();
}

void UMCPScrollGridPopupWidget::RefreshCountText()
{
	if (MCP_CountText != nullptr)
	{
		MCP_CountText->SetText(FText::FromString(FString::Printf(TEXT("Items: %d"), ItemCount)));
	}
}

FMCPScrollGridItemData UMCPScrollGridPopupWidget::CreateRandomItemData(int32 ItemIndex) const
{
	const float Hue01 = FMath::FRand();
	const float Saturation = FMath::FRandRange(0.45f, 0.85f);
	const float Value = FMath::FRandRange(0.55f, 0.90f);

	const uint8 Hue = static_cast<uint8>(FMath::RoundToInt(Hue01 * 255.0f));
	const uint8 Sat = static_cast<uint8>(FMath::RoundToInt(Saturation * 255.0f));
	const uint8 Val = static_cast<uint8>(FMath::RoundToInt(Value * 255.0f));

	FMCPScrollGridItemData Data;
	Data.BackgroundColor = FLinearColor::MakeFromHSV8(Hue, Sat, Val);
	Data.Name = FText::FromString(FString::Printf(TEXT("Item %d"), ItemIndex));
	return Data;
}
