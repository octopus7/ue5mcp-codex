#include "MCPScrollGridItemWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"

void UMCPScrollGridItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildFallbackWidgetTreeIfNeeded();
}

void UMCPScrollGridItemWidget::ApplyItemData(const FMCPScrollGridItemData& InData)
{
	if (MCP_ItemBackground != nullptr)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(InData.BackgroundColor);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			FVector4(12.0f, 12.0f, 12.0f, 12.0f),
			FSlateColor(FLinearColor::Transparent),
			0.0f);
		MCP_ItemBackground->SetBrush(Brush);
		MCP_ItemBackground->SetBrushColor(InData.BackgroundColor);
	}

	if (MCP_ItemNameText != nullptr)
	{
		MCP_ItemNameText->SetText(InData.Name);

		const float Luminance =
			(0.2126f * InData.BackgroundColor.R) +
			(0.7152f * InData.BackgroundColor.G) +
			(0.0722f * InData.BackgroundColor.B);
		const FLinearColor ItemTextColor = (Luminance > 0.60f) ? FLinearColor::Black : FLinearColor::White;
		MCP_ItemNameText->SetColorAndOpacity(FSlateColor(ItemTextColor));
	}
}

void UMCPScrollGridItemWidget::BuildFallbackWidgetTreeIfNeeded()
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
	ItemSizeBox->SetHeightOverride(72.0f);
	WidgetTree->RootWidget = ItemSizeBox;

	MCP_ItemBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MCP_ItemBackground"));
	if (MCP_ItemBackground == nullptr)
	{
		return;
	}
	ItemSizeBox->SetContent(MCP_ItemBackground);

	MCP_ItemBackground->SetPadding(FMargin(10.0f, 8.0f));

	MCP_ItemNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_ItemNameText"));
	if (MCP_ItemNameText == nullptr)
	{
		return;
	}
	MCP_ItemNameText->SetJustification(ETextJustify::Center);
	MCP_ItemNameText->SetText(FText::FromString(TEXT("Item")));
	MCP_ItemBackground->SetContent(MCP_ItemNameText);
}
