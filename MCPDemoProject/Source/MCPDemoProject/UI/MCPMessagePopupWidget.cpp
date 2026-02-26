#include "MCPMessagePopupWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

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
}

void UMCPMessagePopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveWidgets();
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
	}
}
