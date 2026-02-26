#include "MCPMessagePopupWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UMCPMessagePopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyDefaultTexts();

	if (MCP_OkButton != nullptr)
	{
		MCP_OkButton->OnClicked.RemoveDynamic(this, &UMCPMessagePopupWidget::HandleOkClicked);
		MCP_OkButton->OnClicked.AddDynamic(this, &UMCPMessagePopupWidget::HandleOkClicked);
	}

	bCloseBroadcasted = false;

	UE_LOG(LogTemp, Log, TEXT("[MCPPopup] NativeConstruct. Dim=%s Panel=%s OK=%s"),
		MCP_DimBlocker != nullptr ? TEXT("true") : TEXT("false"),
		MCP_PopupPanel != nullptr ? TEXT("true") : TEXT("false"),
		MCP_OkButton != nullptr ? TEXT("true") : TEXT("false"));
}

void UMCPMessagePopupWidget::OpenPopup(const FText& InTitle, const FText& InMessage)
{
	if (MCP_TitleText != nullptr)
	{
		MCP_TitleText->SetText(InTitle);
	}

	if (MCP_MessageText != nullptr)
	{
		MCP_MessageText->SetText(InMessage);
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

void UMCPMessagePopupWidget::ApplyDefaultTexts()
{
	if (MCP_TitleText != nullptr && MCP_TitleText->GetText().IsEmpty())
	{
		MCP_TitleText->SetText(FText::FromString(TEXT("Message Box")));
	}

	if (MCP_MessageText != nullptr && MCP_MessageText->GetText().IsEmpty())
	{
		MCP_MessageText->SetText(FText::FromString(TEXT("This is a simple message popup.")));
	}

	if (MCP_OkLabel != nullptr)
	{
		if (MCP_OkLabel->GetText().IsEmpty())
		{
			MCP_OkLabel->SetText(FText::FromString(TEXT("OK")));
		}
	}
}
