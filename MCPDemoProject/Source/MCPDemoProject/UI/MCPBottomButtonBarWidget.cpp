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

	if (TestTilePopupOpenButton != nullptr)
	{
		TestTilePopupOpenButton->OnClicked.RemoveAll(this);
		TestTilePopupOpenButton->OnClicked.AddDynamic(this, &UMCPBottomButtonBarWidget::HandleTestTilePopupOpenButtonClicked);
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

	ActivePopupWidget = OpenPopupWidget(PopupWidgetClass);
}

void UMCPBottomButtonBarWidget::HandleTestTilePopupOpenButtonClicked()
{
	if (ActiveTilePopupWidget != nullptr && ActiveTilePopupWidget->IsInViewport())
	{
		return;
	}

	ActiveTilePopupWidget = nullptr;

	if (ItemTilePopupWidgetClass == nullptr)
	{
		return;
	}

	ActiveTilePopupWidget = OpenPopupWidget(ItemTilePopupWidgetClass);
}

UMCPPopupWidget* UMCPBottomButtonBarWidget::OpenPopupWidget(const TSubclassOf<UMCPPopupWidget> InPopupWidgetClass) const
{
	if (InPopupWidgetClass == nullptr)
	{
		return nullptr;
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (OwningPlayer == nullptr && GetWorld() != nullptr)
	{
		OwningPlayer = GetWorld()->GetFirstPlayerController();
	}

	if (OwningPlayer == nullptr)
	{
		return nullptr;
	}

	UMCPPopupWidget* const PopupWidget = CreateWidget<UMCPPopupWidget>(OwningPlayer, InPopupWidgetClass);
	if (PopupWidget != nullptr)
	{
		PopupWidget->AddToViewport(100);
	}

	return PopupWidget;
}
