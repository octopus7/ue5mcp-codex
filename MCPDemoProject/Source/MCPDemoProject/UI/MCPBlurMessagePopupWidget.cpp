#include "MCPBlurMessagePopupWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateBrush.h"

void UMCPBlurMessagePopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFallbackWidgetTreeIfNeeded();
	ApplyDefaultTexts();

	if (MCP_OkButton != nullptr)
	{
		MCP_OkButton->OnClicked.RemoveDynamic(this, &UMCPBlurMessagePopupWidget::HandleOkClicked);
		MCP_OkButton->OnClicked.AddDynamic(this, &UMCPBlurMessagePopupWidget::HandleOkClicked);
	}

	bCloseBroadcasted = false;
}

void UMCPBlurMessagePopupWidget::OpenPopup(const FMCPMessagePopupRequest& InRequest)
{
	if (MCP_TitleText != nullptr)
	{
		MCP_TitleText->SetText(InRequest.Title.IsEmpty() ? FText::FromString(TEXT("Message Box")) : InRequest.Title);
	}

	if (MCP_MessageText != nullptr)
	{
		MCP_MessageText->SetText(InRequest.Message.IsEmpty() ? FText::FromString(TEXT("This is a simple message popup.")) : InRequest.Message);
	}

	if (MCP_OkLabel != nullptr)
	{
		MCP_OkLabel->SetText(InRequest.OkLabel.IsEmpty() ? FText::FromString(TEXT("OK")) : InRequest.OkLabel);
	}

	ActiveCloseCallback = InRequest.Callback;
	bCloseBroadcasted = false;
	SetVisibility(ESlateVisibility::Visible);
	SetIsEnabled(true);
}

void UMCPBlurMessagePopupWidget::HandleOkClicked()
{
	if (bCloseBroadcasted)
	{
		return;
	}

	bCloseBroadcasted = true;

	if (ActiveCloseCallback.IsBound())
	{
		ActiveCloseCallback.Execute();
	}
	ActiveCloseCallback.Unbind();

	OnClosed.Broadcast();
	RemoveFromParent();
}

void UMCPBlurMessagePopupWidget::BuildFallbackWidgetTreeIfNeeded()
{
	if (WidgetTree == nullptr)
	{
		return;
	}

	if (MCP_DimBlocker == nullptr)
	{
		MCP_DimBlocker = Cast<UBorder>(WidgetTree->FindWidget(TEXT("MCP_DimBlocker")));
	}
	if (MCP_BlurPanel == nullptr)
	{
		MCP_BlurPanel = Cast<UBackgroundBlur>(WidgetTree->FindWidget(TEXT("MCP_BlurPanel")));
	}
	if (MCP_MessageBox == nullptr)
	{
		MCP_MessageBox = Cast<UBorder>(WidgetTree->FindWidget(TEXT("MCP_MessageBox")));
	}
	if (MCP_TitleText == nullptr)
	{
		MCP_TitleText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_TitleText")));
	}
	if (MCP_MessageText == nullptr)
	{
		MCP_MessageText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_MessageText")));
	}
	if (MCP_OkButton == nullptr)
	{
		MCP_OkButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("MCP_OkButton")));
	}
	if (MCP_OkLabel == nullptr)
	{
		MCP_OkLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_OkLabel")));
	}

	if (MCP_DimBlocker != nullptr &&
		MCP_BlurPanel != nullptr &&
		MCP_MessageBox != nullptr &&
		MCP_TitleText != nullptr &&
		MCP_MessageText != nullptr &&
		MCP_OkButton != nullptr &&
		MCP_OkLabel != nullptr)
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

	MCP_BlurPanel = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("MCP_BlurPanel"));
	if (MCP_BlurPanel == nullptr)
	{
		return;
	}

	MCP_BlurPanel->SetBlurStrength(20.0f);
	MCP_BlurPanel->SetApplyAlphaToBlur(true);
	MCP_BlurPanel->SetCornerRadius(FVector4(16.0f, 16.0f, 16.0f, 16.0f));

	RootCanvas->AddChild(MCP_BlurPanel);
	if (UCanvasPanelSlot* BlurSlot = Cast<UCanvasPanelSlot>(MCP_BlurPanel->Slot))
	{
		BlurSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		BlurSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		BlurSlot->SetSize(FVector2D(560.0f, 320.0f));
		BlurSlot->SetPosition(FVector2D::ZeroVector);
		BlurSlot->SetZOrder(1);
	}

	MCP_MessageBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MCP_MessageBox"));
	if (MCP_MessageBox == nullptr)
	{
		return;
	}

	{
		FSlateBrush MessageBoxBrush;
		MessageBoxBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		MessageBoxBrush.TintColor = FSlateColor(FLinearColor(0.12f, 0.13f, 0.15f, 0.94f));
		MessageBoxBrush.OutlineSettings = FSlateBrushOutlineSettings(
			FVector4(16.0f, 16.0f, 16.0f, 16.0f),
			FSlateColor(FLinearColor(0.24f, 0.25f, 0.29f, 1.0f)),
			1.0f);
		MCP_MessageBox->SetBrush(MessageBoxBrush);
		MCP_MessageBox->SetPadding(FMargin(24.0f));
	}

	MCP_BlurPanel->SetContent(MCP_MessageBox);

	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MCP_ContentVBox"));
	if (ContentVBox == nullptr)
	{
		return;
	}
	MCP_MessageBox->SetContent(ContentVBox);

	MCP_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_TitleText"));
	if (MCP_TitleText != nullptr)
	{
		MCP_TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.93f, 0.96f, 1.0f)));
		MCP_TitleText->SetJustification(ETextJustify::Center);
		ContentVBox->AddChild(MCP_TitleText);

		if (UVerticalBoxSlot* TitleSlot = Cast<UVerticalBoxSlot>(MCP_TitleText->Slot))
		{
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		}
	}

	MCP_MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_MessageText"));
	if (MCP_MessageText != nullptr)
	{
		MCP_MessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.87f, 0.91f, 1.0f)));
		MCP_MessageText->SetJustification(ETextJustify::Center);
		MCP_MessageText->SetAutoWrapText(true);
		ContentVBox->AddChild(MCP_MessageText);

		if (UVerticalBoxSlot* MessageSlot = Cast<UVerticalBoxSlot>(MCP_MessageText->Slot))
		{
			FSlateChildSize FillSize;
			FillSize.SizeRule = ESlateSizeRule::Fill;
			FillSize.Value = 1.0f;
			MessageSlot->SetSize(FillSize);
			MessageSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 16.0f));
		}
	}

	UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MCP_ActionRow"));
	if (ActionRow != nullptr)
	{
		ContentVBox->AddChild(ActionRow);
		if (UVerticalBoxSlot* ActionSlot = Cast<UVerticalBoxSlot>(ActionRow->Slot))
		{
			ActionSlot->SetHorizontalAlignment(HAlign_Right);
		}

		MCP_OkButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MCP_OkButton"));
		if (MCP_OkButton != nullptr)
		{
			ActionRow->AddChildToHorizontalBox(MCP_OkButton);
			if (UHorizontalBoxSlot* OkButtonSlot = Cast<UHorizontalBoxSlot>(MCP_OkButton->Slot))
			{
				OkButtonSlot->SetVerticalAlignment(VAlign_Center);
			}

			MCP_OkLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_OkLabel"));
			if (MCP_OkLabel != nullptr)
			{
				MCP_OkLabel->SetJustification(ETextJustify::Center);
				MCP_OkLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				MCP_OkButton->SetContent(MCP_OkLabel);
			}
		}
	}
}

void UMCPBlurMessagePopupWidget::ApplyDefaultTexts()
{
	if (MCP_TitleText != nullptr && MCP_TitleText->GetText().IsEmpty())
	{
		MCP_TitleText->SetText(FText::FromString(TEXT("Message Box")));
	}

	if (MCP_MessageText != nullptr && MCP_MessageText->GetText().IsEmpty())
	{
		MCP_MessageText->SetText(FText::FromString(TEXT("This is a simple message popup.")));
	}

	if (MCP_OkLabel != nullptr && MCP_OkLabel->GetText().IsEmpty())
	{
		MCP_OkLabel->SetText(FText::FromString(TEXT("OK")));
	}
}
