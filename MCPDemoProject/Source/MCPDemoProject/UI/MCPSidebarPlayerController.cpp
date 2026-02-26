#include "MCPSidebarPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "MCPSidebarWidget.h"

AMCPSidebarPlayerController::AMCPSidebarPlayerController()
{
	SidebarWidgetClass = UMCPSidebarWidget::StaticClass();
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] PlayerController constructed. SidebarWidgetClass=%s"), *GetNameSafe(SidebarWidgetClass));
}

void AMCPSidebarPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] BeginPlay. IsLocalController=%s"), IsLocalController() ? TEXT("true") : TEXT("false"));
	TryCreateSidebarWidget();
}

void AMCPSidebarPlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] BeginPlayingState. IsLocalController=%s"), IsLocalController() ? TEXT("true") : TEXT("false"));
	TryCreateSidebarWidget();
}

void AMCPSidebarPlayerController::TryCreateSidebarWidget()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] TryCreateSidebarWidget called."));

	if (!IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MCPSidebar] Skipped widget creation: controller is not local."));
		return;
	}

	if (SidebarWidgetClass == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] SidebarWidgetClass is null."));
		return;
	}

	if (SidebarWidgetInstance != nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Sidebar widget already exists: %s"), *GetNameSafe(SidebarWidgetInstance));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Creating widget from class: %s"), *GetNameSafe(SidebarWidgetClass));
	SidebarWidgetInstance = CreateWidget<UMCPSidebarWidget>(this, SidebarWidgetClass);
	if (SidebarWidgetInstance != nullptr)
	{
		SidebarWidgetInstance->AddToViewport(100);
		UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Sidebar widget created and added to viewport."));
		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Silver, TEXT("Sidebar widget created"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] CreateWidget failed."));
	}
}
