// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPBottomButtonBarWidget.h"

#include "Components/Button.h"
#include "GameFramework/PlayerController.h"
#include "MCPPopupWidget.h"

void UMCPBottomButtonBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (TestPopupOpenButton != nullptr)
	{
		TestPopupOpenButton->OnClicked.RemoveAll(this);
		TestPopupOpenButton->OnClicked.AddDynamic(this, &UMCPBottomButtonBarWidget::HandleTestPopupOpenButtonClicked);
	}
}

void UMCPBottomButtonBarWidget::HandleTestPopupOpenButtonClicked()
{
	if (ActivePopupWidget != nullptr && ActivePopupWidget->IsInViewport())
	{
		return;
	}

	ActivePopupWidget = nullptr;

	if (PopupWidgetClass == nullptr)
	{
		return;
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (OwningPlayer == nullptr && GetWorld() != nullptr)
	{
		OwningPlayer = GetWorld()->GetFirstPlayerController();
	}

	if (OwningPlayer == nullptr)
	{
		return;
	}

	ActivePopupWidget = CreateWidget<UMCPPopupWidget>(OwningPlayer, PopupWidgetClass);
	if (ActivePopupWidget != nullptr)
	{
		ActivePopupWidget->AddToViewport(100);
	}
}
