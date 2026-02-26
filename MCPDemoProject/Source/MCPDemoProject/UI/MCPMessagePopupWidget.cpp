#include "MCPMessagePopupWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"

namespace
{
template <typename TWidgetType>
TWidgetType* FindNamedWidget(UWidgetTree* WidgetTree, const TCHAR* ManagedName, const TCHAR* LegacyName = nullptr)
{
	if (WidgetTree == nullptr)
	{
		return nullptr;
	}

	if (ManagedName != nullptr)
	{
		if (TWidgetType* Found = Cast<TWidgetType>(WidgetTree->FindWidget(FName(ManagedName))))
		{
			return Found;
		}
	}

	if (LegacyName != nullptr)
	{
		return Cast<TWidgetType>(WidgetTree->FindWidget(FName(LegacyName)));
	}

	return nullptr;
}

void ApplyButtonVisual(UButton* Button)
{
	if (Button == nullptr)
	{
		return;
	}

	FButtonStyle ButtonStyle = Button->GetStyle();
	ButtonStyle.Normal.DrawAs = ESlateBrushDrawType::RoundedBox;
	ButtonStyle.Normal.TintColor = FSlateColor(FLinearColor(0.18f, 0.18f, 0.18f, 0.95f));
	ButtonStyle.Normal.OutlineSettings = FSlateBrushOutlineSettings(8.0f, FSlateColor(FLinearColor::Transparent), 0.0f);

	ButtonStyle.Hovered = ButtonStyle.Normal;
	ButtonStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.24f, 0.24f, 0.24f, 1.0f));

	ButtonStyle.Pressed = ButtonStyle.Normal;
	ButtonStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));

	Button->SetStyle(ButtonStyle);

	if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button->Slot))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonSlot->SetVerticalAlignment(VAlign_Center);
		ButtonSlot->SetPadding(FMargin(10.0f, 8.0f));
	}
}
}

void UMCPMessagePopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveWidgets();
	ApplyVisualStyle();
	ApplyDefaultTexts();

	if (OkButton != nullptr)
	{
		OkButton->OnClicked.RemoveDynamic(this, &UMCPMessagePopupWidget::HandleOkClicked);
		OkButton->OnClicked.AddDynamic(this, &UMCPMessagePopupWidget::HandleOkClicked);
	}

	bCloseBroadcasted = false;

	UE_LOG(LogTemp, Log, TEXT("[MCPPopup] NativeConstruct. Dim=%s Panel=%s OK=%s"),
		DimBlocker != nullptr ? TEXT("true") : TEXT("false"),
		PopupPanel != nullptr ? TEXT("true") : TEXT("false"),
		OkButton != nullptr ? TEXT("true") : TEXT("false"));
}

void UMCPMessagePopupWidget::OpenPopup(const FText& InTitle, const FText& InMessage)
{
	if (TitleText != nullptr)
	{
		TitleText->SetText(InTitle);
	}

	if (MessageText != nullptr)
	{
		MessageText->SetText(InMessage);
	}

	bCloseBroadcasted = false;
	SetVisibility(ESlateVisibility::Visible);
	SetIsEnabled(true);
}

void UMCPMessagePopupWidget::HandleOkClicked()
{
	if (bCloseBroadcasted)
	{
		return;
	}

	bCloseBroadcasted = true;
	OnPopupClosed.Broadcast();
	RemoveFromParent();
}

void UMCPMessagePopupWidget::ResolveWidgets()
{
	DimBlocker = FindNamedWidget<UBorder>(WidgetTree, TEXT("MCP_DimBlocker"), TEXT("DimBlocker"));
	PopupPanel = FindNamedWidget<UBorder>(WidgetTree, TEXT("MCP_PopupPanel"), TEXT("PopupPanel"));
	TitleText = FindNamedWidget<UTextBlock>(WidgetTree, TEXT("MCP_TitleText"), TEXT("TitleText"));
	MessageText = FindNamedWidget<UTextBlock>(WidgetTree, TEXT("MCP_MessageText"), TEXT("MessageText"));
	OkButton = FindNamedWidget<UButton>(WidgetTree, TEXT("MCP_OkButton"), TEXT("OkButton"));
	OkLabel = FindNamedWidget<UTextBlock>(WidgetTree, TEXT("MCP_OkLabel"), TEXT("OkLabel"));
}

void UMCPMessagePopupWidget::ApplyVisualStyle()
{
	if (DimBlocker != nullptr)
	{
		FSlateBrush DimBrush;
		DimBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		DimBrush.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		DimBrush.OutlineSettings = FSlateBrushOutlineSettings(0.0f, FSlateColor(FLinearColor::Transparent), 0.0f);
		DimBlocker->SetBrush(DimBrush);
	}

	if (PopupPanel != nullptr)
	{
		FSlateBrush PanelBrush;
		PanelBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		PanelBrush.TintColor = FSlateColor(FLinearColor(0.06f, 0.06f, 0.06f, 0.95f));
		PanelBrush.OutlineSettings = FSlateBrushOutlineSettings(14.0f, FSlateColor(FLinearColor::Transparent), 0.0f);
		PopupPanel->SetBrush(PanelBrush);
	}

	ApplyButtonVisual(OkButton);
}

void UMCPMessagePopupWidget::ApplyDefaultTexts()
{
	if (TitleText != nullptr && TitleText->GetText().IsEmpty())
	{
		TitleText->SetText(FText::FromString(TEXT("Message Box")));
	}

	if (MessageText != nullptr && MessageText->GetText().IsEmpty())
	{
		MessageText->SetText(FText::FromString(TEXT("This is a simple message popup.")));
	}

	if (OkLabel != nullptr)
	{
		if (OkLabel->GetText().IsEmpty())
		{
			OkLabel->SetText(FText::FromString(TEXT("OK")));
		}
		OkLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		OkLabel->SetJustification(ETextJustify::Center);
	}

	if (TitleText != nullptr)
	{
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}

	if (MessageText != nullptr)
	{
		MessageText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
}
