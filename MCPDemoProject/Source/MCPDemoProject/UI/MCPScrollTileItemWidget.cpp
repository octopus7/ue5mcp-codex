#include "MCPScrollTileItemWidget.h"

#include "MCPScrollTileItemObject.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

void UMCPScrollTileItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildFallbackWidgetTreeIfNeeded();
	ApplySquareTileSize();
	EnsureIconVisualTree();
}

void UMCPScrollTileItemWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	ApplyItemObject(Cast<UMCPScrollTileItemObject>(ListItemObject));
}

void UMCPScrollTileItemWidget::ApplySquareTileSize()
{
	if (MCP_ItemSizeBox == nullptr)
	{
		return;
	}

	constexpr float TileItemSize = 160.0f;
	MCP_ItemSizeBox->SetWidthOverride(TileItemSize);
	MCP_ItemSizeBox->SetHeightOverride(TileItemSize);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MCP_ItemSizeBox->Slot))
	{
		CanvasSlot->SetSize(FVector2D(TileItemSize, TileItemSize));
	}
}

void UMCPScrollTileItemWidget::EnsureIconVisualTree()
{
	if (MCP_ItemBackground == nullptr || WidgetTree == nullptr)
	{
		return;
	}

	constexpr float IconSize = 128.0f;

	if (MCP_ItemIconImage != nullptr)
	{
		if (MCP_ItemIconSizeBox == nullptr)
		{
			MCP_ItemIconSizeBox = Cast<USizeBox>(MCP_ItemIconImage->GetParent());
		}

		if (MCP_ItemIconSizeBox != nullptr)
		{
			MCP_ItemIconSizeBox->SetWidthOverride(IconSize);
			MCP_ItemIconSizeBox->SetHeightOverride(IconSize);
		}

		MCP_ItemIconImage->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
		return;
	}

	UWidget* ExistingContent = MCP_ItemBackground->GetContent();
	if (ExistingContent == nullptr && MCP_ItemNameText != nullptr)
	{
		ExistingContent = MCP_ItemNameText;
	}

	if (ExistingContent == nullptr)
	{
		return;
	}

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MCP_ItemOverlayRuntime"));
	if (Overlay == nullptr)
	{
		return;
	}

	MCP_ItemBackground->SetContent(Overlay);

	MCP_ItemIconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MCP_ItemIconSizeBox"));
	if (MCP_ItemIconSizeBox != nullptr)
	{
		MCP_ItemIconSizeBox->SetWidthOverride(IconSize);
		MCP_ItemIconSizeBox->SetHeightOverride(IconSize);

		MCP_ItemIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MCP_ItemIconImage"));
		if (MCP_ItemIconImage != nullptr)
		{
			MCP_ItemIconImage->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
			MCP_ItemIconSizeBox->SetContent(MCP_ItemIconImage);
		}

		Overlay->AddChildToOverlay(MCP_ItemIconSizeBox);
		if (UOverlaySlot* IconSlot = Cast<UOverlaySlot>(MCP_ItemIconSizeBox->Slot))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
		}
	}

	Overlay->AddChildToOverlay(ExistingContent);
	if (UOverlaySlot* NameSlot = Cast<UOverlaySlot>(ExistingContent->Slot))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Center);
		NameSlot->SetVerticalAlignment(VAlign_Bottom);
		NameSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 10.0f));
	}
}

void UMCPScrollTileItemWidget::BuildFallbackWidgetTreeIfNeeded()
{
	if (MCP_ItemBackground != nullptr && MCP_ItemNameText != nullptr)
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

	USizeBox* ItemSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MCP_ItemSizeBox"));
	if (ItemSizeBox == nullptr)
	{
		return;
	}
	ItemSizeBox->SetWidthOverride(160.0f);
	ItemSizeBox->SetHeightOverride(160.0f);
	WidgetTree->RootWidget = ItemSizeBox;

	MCP_ItemBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MCP_ItemBackground"));
	if (MCP_ItemBackground == nullptr)
	{
		return;
	}
	ItemSizeBox->SetContent(MCP_ItemBackground);
	MCP_ItemBackground->SetPadding(FMargin(10.0f, 8.0f));

	UOverlay* ItemOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MCP_ItemOverlay"));
	if (ItemOverlay == nullptr)
	{
		return;
	}
	MCP_ItemBackground->SetContent(ItemOverlay);

	MCP_ItemIconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MCP_ItemIconSizeBox"));
	if (MCP_ItemIconSizeBox != nullptr)
	{
		MCP_ItemIconSizeBox->SetWidthOverride(128.0f);
		MCP_ItemIconSizeBox->SetHeightOverride(128.0f);

		MCP_ItemIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MCP_ItemIconImage"));
		if (MCP_ItemIconImage != nullptr)
		{
			MCP_ItemIconImage->SetDesiredSizeOverride(FVector2D(128.0f, 128.0f));
			MCP_ItemIconSizeBox->SetContent(MCP_ItemIconImage);
		}

		ItemOverlay->AddChildToOverlay(MCP_ItemIconSizeBox);
		if (UOverlaySlot* IconSlot = Cast<UOverlaySlot>(MCP_ItemIconSizeBox->Slot))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
		}
	}

	MCP_ItemNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_ItemNameText"));
	if (MCP_ItemNameText == nullptr)
	{
		return;
	}
	MCP_ItemNameText->SetJustification(ETextJustify::Center);
	MCP_ItemNameText->SetText(FText::FromString(TEXT("Item")));
	ItemOverlay->AddChildToOverlay(MCP_ItemNameText);
	if (UOverlaySlot* NameSlot = Cast<UOverlaySlot>(MCP_ItemNameText->Slot))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Center);
		NameSlot->SetVerticalAlignment(VAlign_Bottom);
		NameSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 10.0f));
	}
}

void UMCPScrollTileItemWidget::ApplyItemObject(const UMCPScrollTileItemObject* ItemObject)
{
	if (ItemObject == nullptr)
	{
		return;
	}

	if (MCP_ItemIconSizeBox != nullptr)
	{
		MCP_ItemIconSizeBox->SetWidthOverride(128.0f);
		MCP_ItemIconSizeBox->SetHeightOverride(128.0f);
	}

	if (MCP_ItemIconImage != nullptr)
	{
		MCP_ItemIconImage->SetDesiredSizeOverride(FVector2D(128.0f, 128.0f));
	}

	if (MCP_ItemBackground != nullptr)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(ItemObject->BackgroundColor);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			FVector4(12.0f, 12.0f, 12.0f, 12.0f),
			FSlateColor(FLinearColor::Transparent),
			0.0f);
		MCP_ItemBackground->SetBrush(Brush);
		MCP_ItemBackground->SetBrushColor(ItemObject->BackgroundColor);
	}

	if (MCP_ItemNameText != nullptr)
	{
		MCP_ItemNameText->SetText(ItemObject->Name);

		const float Luminance =
			(0.2126f * ItemObject->BackgroundColor.R) +
			(0.7152f * ItemObject->BackgroundColor.G) +
			(0.0722f * ItemObject->BackgroundColor.B);
		const FLinearColor ItemTextColor = (Luminance > 0.60f) ? FLinearColor::Black : FLinearColor::White;
		MCP_ItemNameText->SetColorAndOpacity(FSlateColor(ItemTextColor));
	}

	if (MCP_ItemIconImage != nullptr)
	{
		if (ItemObject->IconTexture != nullptr)
		{
			MCP_ItemIconImage->SetBrushFromTexture(ItemObject->IconTexture, false);
			MCP_ItemIconImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			MCP_ItemIconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
