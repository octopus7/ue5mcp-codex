// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPPopupWidget.h"

#include "Components/Button.h"

void UMCPPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CloseButton != nullptr)
	{
		CloseButton->OnClicked.RemoveAll(this);
		CloseButton->OnClicked.AddDynamic(this, &UMCPPopupWidget::HandleCloseButtonClicked);
	}
}

void UMCPPopupWidget::HandleCloseButtonClicked()
{
	RemoveFromParent();
}
