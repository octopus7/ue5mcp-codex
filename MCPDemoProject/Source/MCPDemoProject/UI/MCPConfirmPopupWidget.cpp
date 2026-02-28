#include "MCPConfirmPopupWidget.h"

#include "Blueprint/WidgetTree.h"
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

void UMCPConfirmPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFallbackWidgetTreeIfNeeded();
	ApplyDefaultTexts();

	if (MCP_ConfirmButton != nullptr)
	{
		MCP_ConfirmButton->OnClicked.RemoveDynamic(this, &UMCPConfirmPopupWidget::HandleConfirmClicked);
		MCP_ConfirmButton->OnClicked.AddDynamic(this, &UMCPConfirmPopupWidget::HandleConfirmClicked);
	}

	if (MCP_CancelButton != nullptr)
	{
		MCP_CancelButton->OnClicked.RemoveDynamic(this, &UMCPConfirmPopupWidget::HandleCancelClicked);
		MCP_CancelButton->OnClicked.AddDynamic(this, &UMCPConfirmPopupWidget::HandleCancelClicked);
	}

	bCloseBroadcasted = false;
}

void UMCPConfirmPopupWidget::OpenPopup(const FMCPConfirmPopupRequest& InRequest)
{
	if (MCP_TitleText != nullptr)
	{
		MCP_TitleText->SetText(InRequest.Title.IsEmpty() ? FText::FromString(TEXT("Confirm")) : InRequest.Title);
	}

	if (MCP_MessageText != nullptr)
	{
		MCP_MessageText->SetText(InRequest.Message.IsEmpty() ? FText::FromString(TEXT("Are you sure?")) : InRequest.Message);
	}

	if (MCP_ConfirmLabel != nullptr)
	{
		MCP_ConfirmLabel->SetText(InRequest.ConfirmLabel.IsEmpty() ? FText::FromString(TEXT("Confirm")) : InRequest.ConfirmLabel);
	}

	if (MCP_CancelLabel != nullptr)
	{
		MCP_CancelLabel->SetText(InRequest.CancelLabel.IsEmpty() ? FText::FromString(TEXT("Cancel")) : InRequest.CancelLabel);
	}

	bCloseBroadcasted = false;
	SetVisibility(ESlateVisibility::Visible);
	SetIsEnabled(true);
}

void UMCPConfirmPopupWidget::HandleConfirmClicked()
{
	if (bCloseBroadcasted)
	{
		return;
	}

	bCloseBroadcasted = true;
	OnConfirmed.Broadcast();
	RemoveFromParent();
}

void UMCPConfirmPopupWidget::HandleCancelClicked()
{
	if (bCloseBroadcasted)
	{
		return;
	}

	bCloseBroadcasted = true;
	OnCancelled.Broadcast();
	RemoveFromParent();
}

void UMCPConfirmPopupWidget::BuildFallbackWidgetTreeIfNeeded()
{
	if (WidgetTree == nullptr)
	{
		return;
	}

	// Try to recover expected references from an existing blueprint tree first.
	if (MCP_DimBlocker == nullptr)
	{
		MCP_DimBlocker = Cast<UBorder>(WidgetTree->FindWidget(TEXT("MCP_DimBlocker")));
	}
	if (MCP_PopupPanel == nullptr)
	{
		MCP_PopupPanel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("MCP_PopupPanel")));
	}
	if (MCP_TitleText == nullptr)
	{
		MCP_TitleText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_TitleText")));
	}
	if (MCP_MessageText == nullptr)
	{
		MCP_MessageText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_MessageText")));
	}
	if (MCP_ConfirmButton == nullptr)
	{
		MCP_ConfirmButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("MCP_ConfirmButton")));
	}
	if (MCP_ConfirmLabel == nullptr)
	{
		MCP_ConfirmLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_ConfirmLabel")));
	}
	if (MCP_CancelButton == nullptr)
	{
		MCP_CancelButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("MCP_CancelButton")));
	}
	if (MCP_CancelLabel == nullptr)
	{
		MCP_CancelLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_CancelLabel")));
	}

	if (MCP_DimBlocker != nullptr &&
		MCP_PopupPanel != nullptr &&
		MCP_TitleText != nullptr &&
		MCP_MessageText != nullptr &&
		MCP_ConfirmButton != nullptr &&
		MCP_ConfirmLabel != nullptr &&
		MCP_CancelButton != nullptr &&
		MCP_CancelLabel != nullptr)
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
		PopupSlot->SetSize(FVector2D(560.0f, 320.0f));
		PopupSlot->SetPosition(FVector2D::ZeroVector);
		PopupSlot->SetZOrder(1);
	}

	UVerticalBox* PopupVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MCP_PopupVerticalBox"));
	if (PopupVerticalBox == nullptr)
	{
		return;
	}
	MCP_PopupPanel->SetContent(PopupVerticalBox);

	MCP_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_TitleText"));
	if (MCP_TitleText != nullptr)
	{
		MCP_TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.93f, 0.96f, 1.0f)));
		MCP_TitleText->SetJustification(ETextJustify::Center);
		PopupVerticalBox->AddChild(MCP_TitleText);
		if (UVerticalBoxSlot* TitleSlot = Cast<UVerticalBoxSlot>(MCP_TitleText->Slot))
		{
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}
	}

	MCP_MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_MessageText"));
	if (MCP_MessageText != nullptr)
	{
		MCP_MessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.87f, 0.91f, 1.0f)));
		MCP_MessageText->SetJustification(ETextJustify::Center);
		MCP_MessageText->SetAutoWrapText(true);
		PopupVerticalBox->AddChild(MCP_MessageText);
		if (UVerticalBoxSlot* MessageSlot = Cast<UVerticalBoxSlot>(MCP_MessageText->Slot))
		{
			FSlateChildSize FillSize;
			FillSize.SizeRule = ESlateSizeRule::Fill;
			FillSize.Value = 1.0f;
			MessageSlot->SetSize(FillSize);
			MessageSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 16.0f));
		}
	}

	UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MCP_ActionRow"));
	if (ActionRow != nullptr)
	{
		PopupVerticalBox->AddChild(ActionRow);
		if (UVerticalBoxSlot* ActionSlot = Cast<UVerticalBoxSlot>(ActionRow->Slot))
		{
			ActionSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
			ActionSlot->SetHorizontalAlignment(HAlign_Right);
		}

		MCP_CancelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MCP_CancelButton"));
		if (MCP_CancelButton != nullptr)
		{
			ActionRow->AddChildToHorizontalBox(MCP_CancelButton);
			if (UHorizontalBoxSlot* CancelButtonSlot = Cast<UHorizontalBoxSlot>(MCP_CancelButton->Slot))
			{
				CancelButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
				CancelButtonSlot->SetVerticalAlignment(VAlign_Center);
			}

			MCP_CancelLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_CancelLabel"));
			if (MCP_CancelLabel != nullptr)
			{
				MCP_CancelLabel->SetJustification(ETextJustify::Center);
				MCP_CancelLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				MCP_CancelButton->SetContent(MCP_CancelLabel);
			}
		}

		MCP_ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MCP_ConfirmButton"));
		if (MCP_ConfirmButton != nullptr)
		{
			ActionRow->AddChildToHorizontalBox(MCP_ConfirmButton);
			if (UHorizontalBoxSlot* ConfirmButtonSlot = Cast<UHorizontalBoxSlot>(MCP_ConfirmButton->Slot))
			{
				ConfirmButtonSlot->SetVerticalAlignment(VAlign_Center);
			}

			MCP_ConfirmLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_ConfirmLabel"));
			if (MCP_ConfirmLabel != nullptr)
			{
				MCP_ConfirmLabel->SetJustification(ETextJustify::Center);
				MCP_ConfirmLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				MCP_ConfirmButton->SetContent(MCP_ConfirmLabel);
			}
		}
	}
}

void UMCPConfirmPopupWidget::ApplyDefaultTexts()
{
	if (MCP_TitleText != nullptr && MCP_TitleText->GetText().IsEmpty())
	{
		MCP_TitleText->SetText(FText::FromString(TEXT("Confirm")));
	}

	if (MCP_MessageText != nullptr && MCP_MessageText->GetText().IsEmpty())
	{
		MCP_MessageText->SetText(FText::FromString(TEXT("Are you sure?")));
	}

	if (MCP_ConfirmLabel != nullptr && MCP_ConfirmLabel->GetText().IsEmpty())
	{
		MCP_ConfirmLabel->SetText(FText::FromString(TEXT("Confirm")));
	}

	if (MCP_CancelLabel != nullptr && MCP_CancelLabel->GetText().IsEmpty())
	{
		MCP_CancelLabel->SetText(FText::FromString(TEXT("Cancel")));
	}
}
