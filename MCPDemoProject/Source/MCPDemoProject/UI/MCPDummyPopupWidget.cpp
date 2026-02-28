#include "MCPDummyPopupWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void UMCPDummyPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFallbackWidgetTreeIfNeeded();
	ApplyDefaultTexts();

	if (MCP_CloseButton != nullptr)
	{
		MCP_CloseButton->OnClicked.RemoveDynamic(this, &UMCPDummyPopupWidget::HandleCloseClicked);
		MCP_CloseButton->OnClicked.AddDynamic(this, &UMCPDummyPopupWidget::HandleCloseClicked);
	}

	bCloseBroadcasted = false;
}

void UMCPDummyPopupWidget::OpenPopup(const FText& InTitle, const FText& InMessage)
{
	if (MCP_TitleText != nullptr)
	{
		MCP_TitleText->SetText(InTitle.IsEmpty() ? FText::FromString(TEXT("Dummy Popup")) : InTitle);
	}

	if (MCP_MessageText != nullptr)
	{
		MCP_MessageText->SetText(InMessage.IsEmpty() ? FText::FromString(TEXT("Dummy popup content")) : InMessage);
	}

	bCloseBroadcasted = false;
	SetVisibility(ESlateVisibility::Visible);
	SetIsEnabled(true);
}

void UMCPDummyPopupWidget::HandleCloseClicked()
{
	if (bCloseBroadcasted)
	{
		return;
	}

	bCloseBroadcasted = true;
	OnClosed.Broadcast();
	RemoveFromParent();
}

void UMCPDummyPopupWidget::BuildFallbackWidgetTreeIfNeeded()
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
	if (MCP_IdentityTintPanel == nullptr)
	{
		MCP_IdentityTintPanel = Cast<UBorder>(WidgetTree->FindWidget(TEXT("MCP_IdentityTintPanel")));
	}
	if (MCP_TitleText == nullptr)
	{
		MCP_TitleText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_TitleText")));
	}
	if (MCP_MessageText == nullptr)
	{
		MCP_MessageText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_MessageText")));
	}
	if (MCP_CloseButton == nullptr)
	{
		MCP_CloseButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("MCP_CloseButton")));
	}
	if (MCP_CloseLabel == nullptr)
	{
		MCP_CloseLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_CloseLabel")));
	}

	if (MCP_DimBlocker != nullptr &&
		MCP_BlurPanel != nullptr &&
		MCP_MessageBox != nullptr &&
		MCP_IdentityTintPanel != nullptr &&
		MCP_TitleText != nullptr &&
		MCP_MessageText != nullptr &&
		MCP_CloseButton != nullptr &&
		MCP_CloseLabel != nullptr)
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
		RootCanvas->AddChild(MCP_DimBlocker);
	}

	MCP_BlurPanel = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("MCP_BlurPanel"));
	if (MCP_BlurPanel == nullptr)
	{
		return;
	}
	RootCanvas->AddChild(MCP_BlurPanel);

	MCP_MessageBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MCP_MessageBox"));
	if (MCP_MessageBox == nullptr)
	{
		return;
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
		ContentVBox->AddChild(MCP_TitleText);
	}

	MCP_IdentityTintPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MCP_IdentityTintPanel"));
	if (MCP_IdentityTintPanel != nullptr)
	{
		ContentVBox->AddChild(MCP_IdentityTintPanel);
	}

	MCP_MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_MessageText"));
	if (MCP_MessageText != nullptr)
	{
		MCP_MessageText->SetAutoWrapText(true);
		ContentVBox->AddChild(MCP_MessageText);
	}

	UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MCP_ActionRow"));
	if (ActionRow != nullptr)
	{
		ContentVBox->AddChild(ActionRow);

		MCP_CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MCP_CloseButton"));
		if (MCP_CloseButton != nullptr)
		{
			ActionRow->AddChildToHorizontalBox(MCP_CloseButton);

			MCP_CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_CloseLabel"));
			if (MCP_CloseLabel != nullptr)
			{
				MCP_CloseButton->SetContent(MCP_CloseLabel);
			}
		}
	}
}

void UMCPDummyPopupWidget::ApplyDefaultTexts()
{
	if (MCP_TitleText != nullptr && MCP_TitleText->GetText().IsEmpty())
	{
		MCP_TitleText->SetText(FText::FromString(TEXT("Dummy Popup")));
	}

	if (MCP_MessageText != nullptr && MCP_MessageText->GetText().IsEmpty())
	{
		MCP_MessageText->SetText(FText::FromString(TEXT("Dummy popup content")));
	}

	if (MCP_CloseLabel != nullptr && MCP_CloseLabel->GetText().IsEmpty())
	{
		MCP_CloseLabel->SetText(FText::FromString(TEXT("Close")));
	}
}
