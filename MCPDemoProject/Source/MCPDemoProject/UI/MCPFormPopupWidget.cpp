#include "MCPFormPopupWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateBrush.h"

namespace
{
FString GetTrimmedNameValue(const UEditableTextBox* NameInput)
{
	if (NameInput == nullptr)
	{
		return FString();
	}

	FString NameValue = NameInput->GetText().ToString();
	NameValue.TrimStartAndEndInline();
	return NameValue;
}
}

void UMCPFormPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFallbackWidgetTreeIfNeeded();
	ApplyDefaultTexts();

	if (MCP_SubmitButton != nullptr)
	{
		MCP_SubmitButton->OnClicked.RemoveDynamic(this, &UMCPFormPopupWidget::HandleSubmitClicked);
		MCP_SubmitButton->OnClicked.AddDynamic(this, &UMCPFormPopupWidget::HandleSubmitClicked);
	}

	if (MCP_CancelButton != nullptr)
	{
		MCP_CancelButton->OnClicked.RemoveDynamic(this, &UMCPFormPopupWidget::HandleCancelClicked);
		MCP_CancelButton->OnClicked.AddDynamic(this, &UMCPFormPopupWidget::HandleCancelClicked);
	}

	if (MCP_NameInput != nullptr)
	{
		MCP_NameInput->OnTextChanged.RemoveDynamic(this, &UMCPFormPopupWidget::HandleNameTextChanged);
		MCP_NameInput->OnTextChanged.AddDynamic(this, &UMCPFormPopupWidget::HandleNameTextChanged);
	}

	bCloseBroadcasted = false;
	UpdateSubmitButtonEnabledState();
}

void UMCPFormPopupWidget::OpenPopup(const FMCPFormPopupRequest& Request)
{
	if (MCP_TitleText != nullptr)
	{
		MCP_TitleText->SetText(Request.Title.IsEmpty() ? FText::FromString(TEXT("Form Sample Popup")) : Request.Title);
	}

	if (MCP_MessageText != nullptr)
	{
		MCP_MessageText->SetText(Request.Message.IsEmpty() ? FText::FromString(TEXT("Fill the form and submit.")) : Request.Message);
	}

	if (MCP_NameInput != nullptr)
	{
		MCP_NameInput->SetHintText(Request.NameHint.IsEmpty() ? FText::FromString(TEXT("Enter name")) : Request.NameHint);
		MCP_NameInput->SetText(FText::GetEmpty());
	}

	if (MCP_CheckLabel != nullptr)
	{
		MCP_CheckLabel->SetText(Request.CheckLabel.IsEmpty() ? FText::FromString(TEXT("Enable Option")) : Request.CheckLabel);
	}

	if (MCP_CheckBox != nullptr)
	{
		MCP_CheckBox->SetIsChecked(false);
	}

	PopulateComboOptions(Request);

	if (MCP_SubmitLabel != nullptr)
	{
		MCP_SubmitLabel->SetText(Request.SubmitLabel.IsEmpty() ? FText::FromString(TEXT("Submit")) : Request.SubmitLabel);
	}

	if (MCP_CancelLabel != nullptr)
	{
		MCP_CancelLabel->SetText(Request.CancelLabel.IsEmpty() ? FText::FromString(TEXT("Cancel")) : Request.CancelLabel);
	}

	bCloseBroadcasted = false;
	UpdateSubmitButtonEnabledState();
	SetVisibility(ESlateVisibility::Visible);
	SetIsEnabled(true);
}

void UMCPFormPopupWidget::HandleSubmitClicked()
{
	if (bCloseBroadcasted)
	{
		return;
	}

	UpdateSubmitButtonEnabledState();
	if (MCP_SubmitButton != nullptr && !MCP_SubmitButton->GetIsEnabled())
	{
		return;
	}

	FMCPFormPopupResult Result;
	CollectCurrentResult(Result, true);

	bCloseBroadcasted = true;
	OnSubmitted.Broadcast(Result);
	RemoveFromParent();
}

void UMCPFormPopupWidget::HandleCancelClicked()
{
	if (bCloseBroadcasted)
	{
		return;
	}

	FMCPFormPopupResult Result;
	CollectCurrentResult(Result, false);

	bCloseBroadcasted = true;
	OnCancelled.Broadcast(Result);
	RemoveFromParent();
}

void UMCPFormPopupWidget::HandleNameTextChanged(const FText& NewText)
{
	UpdateSubmitButtonEnabledState();
}

void UMCPFormPopupWidget::BuildFallbackWidgetTreeIfNeeded()
{
	if (WidgetTree == nullptr)
	{
		return;
	}

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
	if (MCP_NameInput == nullptr)
	{
		MCP_NameInput = Cast<UEditableTextBox>(WidgetTree->FindWidget(TEXT("MCP_NameInput")));
	}
	if (MCP_CheckBox == nullptr)
	{
		MCP_CheckBox = Cast<UCheckBox>(WidgetTree->FindWidget(TEXT("MCP_CheckBox")));
	}
	if (MCP_CheckLabel == nullptr)
	{
		MCP_CheckLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_CheckLabel")));
	}
	if (MCP_ComboBox == nullptr)
	{
		MCP_ComboBox = Cast<UComboBoxString>(WidgetTree->FindWidget(TEXT("MCP_ComboBox")));
	}
	if (MCP_SubmitButton == nullptr)
	{
		MCP_SubmitButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("MCP_SubmitButton")));
	}
	if (MCP_SubmitLabel == nullptr)
	{
		MCP_SubmitLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("MCP_SubmitLabel")));
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
		MCP_NameInput != nullptr &&
		MCP_CheckBox != nullptr &&
		MCP_CheckLabel != nullptr &&
		MCP_ComboBox != nullptr &&
		MCP_SubmitButton != nullptr &&
		MCP_SubmitLabel != nullptr &&
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
		PopupSlot->SetSize(FVector2D(620.0f, 420.0f));
		PopupSlot->SetPosition(FVector2D::ZeroVector);
		PopupSlot->SetZOrder(1);
	}

	UVerticalBox* PopupVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MCP_FormVBox"));
	if (PopupVerticalBox == nullptr)
	{
		return;
	}
	MCP_PopupPanel->SetContent(PopupVerticalBox);

	MCP_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_TitleText"));
	if (MCP_TitleText != nullptr)
	{
		MCP_TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.93f, 0.96f, 1.0f)));
		PopupVerticalBox->AddChild(MCP_TitleText);
		if (UVerticalBoxSlot* TitleTextSlot = Cast<UVerticalBoxSlot>(MCP_TitleText->Slot))
		{
			TitleTextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}
	}

	MCP_MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_MessageText"));
	if (MCP_MessageText != nullptr)
	{
		MCP_MessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.87f, 0.91f, 1.0f)));
		MCP_MessageText->SetAutoWrapText(true);
		PopupVerticalBox->AddChild(MCP_MessageText);
		if (UVerticalBoxSlot* MessageTextSlot = Cast<UVerticalBoxSlot>(MCP_MessageText->Slot))
		{
			MessageTextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
		}
	}

	MCP_NameInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("MCP_NameInput"));
	if (MCP_NameInput != nullptr)
	{
		PopupVerticalBox->AddChild(MCP_NameInput);
		if (UVerticalBoxSlot* NameInputSlot = Cast<UVerticalBoxSlot>(MCP_NameInput->Slot))
		{
			NameInputSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		}
	}

	UHorizontalBox* CheckRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MCP_CheckRow"));
	if (CheckRow != nullptr)
	{
		PopupVerticalBox->AddChild(CheckRow);
		if (UVerticalBoxSlot* CheckRowSlot = Cast<UVerticalBoxSlot>(CheckRow->Slot))
		{
			CheckRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		}

		MCP_CheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("MCP_CheckBox"));
		if (MCP_CheckBox != nullptr)
		{
			CheckRow->AddChildToHorizontalBox(MCP_CheckBox);
			if (UHorizontalBoxSlot* CheckSlot = Cast<UHorizontalBoxSlot>(MCP_CheckBox->Slot))
			{
				CheckSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
				CheckSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		MCP_CheckLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_CheckLabel"));
		if (MCP_CheckLabel != nullptr)
		{
			MCP_CheckLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.91f, 0.94f, 1.0f)));
			CheckRow->AddChildToHorizontalBox(MCP_CheckLabel);
			if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(MCP_CheckLabel->Slot))
			{
				LabelSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}

	MCP_ComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("MCP_ComboBox"));
	if (MCP_ComboBox != nullptr)
	{
		PopupVerticalBox->AddChild(MCP_ComboBox);
		if (UVerticalBoxSlot* ComboBoxSlot = Cast<UVerticalBoxSlot>(MCP_ComboBox->Slot))
		{
			ComboBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
		}
	}

	UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MCP_ActionRow"));
	if (ActionRow != nullptr)
	{
		PopupVerticalBox->AddChild(ActionRow);
		if (UVerticalBoxSlot* ActionSlot = Cast<UVerticalBoxSlot>(ActionRow->Slot))
		{
			ActionSlot->SetHorizontalAlignment(HAlign_Right);
			ActionSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
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

		MCP_SubmitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MCP_SubmitButton"));
		if (MCP_SubmitButton != nullptr)
		{
			ActionRow->AddChildToHorizontalBox(MCP_SubmitButton);
			if (UHorizontalBoxSlot* SubmitButtonSlot = Cast<UHorizontalBoxSlot>(MCP_SubmitButton->Slot))
			{
				SubmitButtonSlot->SetVerticalAlignment(VAlign_Center);
			}

			MCP_SubmitLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MCP_SubmitLabel"));
			if (MCP_SubmitLabel != nullptr)
			{
				MCP_SubmitLabel->SetJustification(ETextJustify::Center);
				MCP_SubmitLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				MCP_SubmitButton->SetContent(MCP_SubmitLabel);
			}
		}
	}
}

void UMCPFormPopupWidget::ApplyDefaultTexts()
{
	if (MCP_TitleText != nullptr && MCP_TitleText->GetText().IsEmpty())
	{
		MCP_TitleText->SetText(FText::FromString(TEXT("Form Sample Popup")));
	}

	if (MCP_MessageText != nullptr && MCP_MessageText->GetText().IsEmpty())
	{
		MCP_MessageText->SetText(FText::FromString(TEXT("Fill the form and submit.")));
	}

	if (MCP_NameInput != nullptr && MCP_NameInput->GetHintText().IsEmpty())
	{
		MCP_NameInput->SetHintText(FText::FromString(TEXT("Enter name")));
	}

	if (MCP_CheckLabel != nullptr && MCP_CheckLabel->GetText().IsEmpty())
	{
		MCP_CheckLabel->SetText(FText::FromString(TEXT("Enable Option")));
	}

	if (MCP_SubmitLabel != nullptr && MCP_SubmitLabel->GetText().IsEmpty())
	{
		MCP_SubmitLabel->SetText(FText::FromString(TEXT("Submit")));
	}

	if (MCP_CancelLabel != nullptr && MCP_CancelLabel->GetText().IsEmpty())
	{
		MCP_CancelLabel->SetText(FText::FromString(TEXT("Cancel")));
	}
}

void UMCPFormPopupWidget::PopulateComboOptions(const FMCPFormPopupRequest& Request)
{
	if (MCP_ComboBox == nullptr)
	{
		return;
	}

	TArray<FString> EffectiveOptions = Request.ComboOptions;
	if (EffectiveOptions.IsEmpty())
	{
		EffectiveOptions = { TEXT("Option A"), TEXT("Option B"), TEXT("Option C") };
	}

	MCP_ComboBox->ClearOptions();
	for (const FString& Option : EffectiveOptions)
	{
		MCP_ComboBox->AddOption(Option);
	}

	if (!EffectiveOptions.IsEmpty())
	{
		const int32 SafeIndex = FMath::Clamp(Request.DefaultComboIndex, 0, EffectiveOptions.Num() - 1);
		MCP_ComboBox->SetSelectedOption(EffectiveOptions[SafeIndex]);
	}
}

void UMCPFormPopupWidget::UpdateSubmitButtonEnabledState()
{
	if (MCP_SubmitButton == nullptr)
	{
		return;
	}

	const bool bHasNameValue = !GetTrimmedNameValue(MCP_NameInput).IsEmpty();
	MCP_SubmitButton->SetIsEnabled(bHasNameValue);
}

void UMCPFormPopupWidget::CollectCurrentResult(FMCPFormPopupResult& OutResult, bool bSubmitted) const
{
	OutResult.bSubmitted = bSubmitted;
	OutResult.NameValue = FText::FromString(GetTrimmedNameValue(MCP_NameInput));
	OutResult.bChecked = MCP_CheckBox != nullptr ? MCP_CheckBox->IsChecked() : false;
	OutResult.SelectedOption = MCP_ComboBox != nullptr ? MCP_ComboBox->GetSelectedOption() : FString();
}
