// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPDemoGameMode.h"

#include "GameFramework/PlayerController.h"
#include "UI/MCPBottomButtonBarWidget.h"

void AMCPDemoGameMode::StartPlay()
{
	Super::StartPlay();

	if (BottomButtonBarWidgetInstance != nullptr && BottomButtonBarWidgetInstance->IsInViewport())
	{
		return;
	}

	if (BottomButtonBarWidgetClass == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	APlayerController* const PlayerController = GetWorld()->GetFirstPlayerController();
	if (PlayerController == nullptr)
	{
		return;
	}

	BottomButtonBarWidgetInstance = CreateWidget<UMCPBottomButtonBarWidget>(PlayerController, BottomButtonBarWidgetClass);
	if (BottomButtonBarWidgetInstance != nullptr)
	{
		BottomButtonBarWidgetInstance->AddToViewport();
	}
}
