#include "MCPABValuePopupWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"

namespace
{
constexpr int32 ABPopupStep = 1;
}

void UMCPABValuePopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MCP_CloseButton != nullptr)
	{
		MCP_CloseButton->OnClicked.RemoveDynamic(this, &UMCPABValuePopupWidget::HandleCloseClicked);
		MCP_CloseButton->OnClicked.AddDynamic(this, &UMCPABValuePopupWidget::HandleCloseClicked);
	}

	if (MCP_ADecButton != nullptr)
	{
		MCP_ADecButton->OnClicked.RemoveDynamic(this, &UMCPABValuePopupWidget::HandleADecClicked);
		MCP_ADecButton->OnClicked.AddDynamic(this, &UMCPABValuePopupWidget::HandleADecClicked);
	}

	if (MCP_AIncButton != nullptr)
	{
		MCP_AIncButton->OnClicked.RemoveDynamic(this, &UMCPABValuePopupWidget::HandleAIncClicked);
		MCP_AIncButton->OnClicked.AddDynamic(this, &UMCPABValuePopupWidget::HandleAIncClicked);
	}

	if (MCP_BDecButton != nullptr)
	{
		MCP_BDecButton->OnClicked.RemoveDynamic(this, &UMCPABValuePopupWidget::HandleBDecClicked);
		MCP_BDecButton->OnClicked.AddDynamic(this, &UMCPABValuePopupWidget::HandleBDecClicked);
	}

	if (MCP_BIncButton != nullptr)
	{
		MCP_BIncButton->OnClicked.RemoveDynamic(this, &UMCPABValuePopupWidget::HandleBIncClicked);
		MCP_BIncButton->OnClicked.AddDynamic(this, &UMCPABValuePopupWidget::HandleBIncClicked);
	}

	if (MCP_ConfirmButton != nullptr)
	{
		MCP_ConfirmButton->OnClicked.RemoveDynamic(this, &UMCPABValuePopupWidget::HandleConfirmClicked);
		MCP_ConfirmButton->OnClicked.AddDynamic(this, &UMCPABValuePopupWidget::HandleConfirmClicked);
	}

	if (MCP_CancelButton != nullptr)
	{
		MCP_CancelButton->OnClicked.RemoveDynamic(this, &UMCPABValuePopupWidget::HandleCancelClicked);
		MCP_CancelButton->OnClicked.AddDynamic(this, &UMCPABValuePopupWidget::HandleCancelClicked);
	}

	ApplyDefaultTexts();
	CurrentA = ClampToRange(CurrentA);
	CurrentB = ClampToRange(CurrentB);
	RefreshValueTexts();
	bCloseBroadcasted = false;
}

void UMCPABValuePopupWidget::OpenPopup(int32 InInitialA, int32 InInitialB)
{
	CurrentA = ClampToRange(InInitialA);
	CurrentB = ClampToRange(InInitialB);
	bCloseBroadcasted = false;
	RefreshValueTexts();

	SetVisibility(ESlateVisibility::Visible);
	SetIsEnabled(true);
}

void UMCPABValuePopupWidget::HandleCloseClicked()
{
	if (bCloseBroadcasted)
	{
		return;
	}

	bCloseBroadcasted = true;
	OnABPopupCancelled.Broadcast();
	RemoveFromParent();
}

void UMCPABValuePopupWidget::HandleADecClicked()
{
	CurrentA = ClampToRange(CurrentA - ABPopupStep);
	RefreshValueTexts();
}

void UMCPABValuePopupWidget::HandleAIncClicked()
{
	CurrentA = ClampToRange(CurrentA + ABPopupStep);
	RefreshValueTexts();
}

void UMCPABValuePopupWidget::HandleBDecClicked()
{
	CurrentB = ClampToRange(CurrentB - ABPopupStep);
	RefreshValueTexts();
}

void UMCPABValuePopupWidget::HandleBIncClicked()
{
	CurrentB = ClampToRange(CurrentB + ABPopupStep);
	RefreshValueTexts();
}

void UMCPABValuePopupWidget::HandleConfirmClicked()
{
	if (bCloseBroadcasted)
	{
		return;
	}

	bCloseBroadcasted = true;
	OnABPopupConfirmed.Broadcast(CurrentA, CurrentB);
	RemoveFromParent();
}

void UMCPABValuePopupWidget::HandleCancelClicked()
{
	HandleCloseClicked();
}

void UMCPABValuePopupWidget::ApplyDefaultTexts()
{
	if (MCP_CloseLabel != nullptr && MCP_CloseLabel->GetText().IsEmpty())
	{
		MCP_CloseLabel->SetText(FText::FromString(TEXT("X")));
	}

	if (MCP_ALabel != nullptr && MCP_ALabel->GetText().IsEmpty())
	{
		MCP_ALabel->SetText(FText::FromString(TEXT("A")));
	}

	if (MCP_BLabel != nullptr && MCP_BLabel->GetText().IsEmpty())
	{
		MCP_BLabel->SetText(FText::FromString(TEXT("B")));
	}

	if (MCP_ADecLabel != nullptr && MCP_ADecLabel->GetText().IsEmpty())
	{
		MCP_ADecLabel->SetText(FText::FromString(TEXT("-")));
	}

	if (MCP_AIncLabel != nullptr && MCP_AIncLabel->GetText().IsEmpty())
	{
		MCP_AIncLabel->SetText(FText::FromString(TEXT("+")));
	}

	if (MCP_BDecLabel != nullptr && MCP_BDecLabel->GetText().IsEmpty())
	{
		MCP_BDecLabel->SetText(FText::FromString(TEXT("-")));
	}

	if (MCP_BIncLabel != nullptr && MCP_BIncLabel->GetText().IsEmpty())
	{
		MCP_BIncLabel->SetText(FText::FromString(TEXT("+")));
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

void UMCPABValuePopupWidget::RefreshValueTexts()
{
	if (MCP_AValueText != nullptr)
	{
		MCP_AValueText->SetText(FText::AsNumber(CurrentA));
	}

	if (MCP_BValueText != nullptr)
	{
		MCP_BValueText->SetText(FText::AsNumber(CurrentB));
	}
}

int32 UMCPABValuePopupWidget::ClampToRange(int32 InValue) const
{
	const int32 LowerBound = FMath::Min(MinValue, MaxValue);
	const int32 UpperBound = FMath::Max(MinValue, MaxValue);
	return FMath::Clamp(InValue, LowerBound, UpperBound);
}
