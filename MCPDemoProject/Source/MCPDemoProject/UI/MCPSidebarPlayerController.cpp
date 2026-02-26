#include "MCPSidebarPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "MCPSidebarWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/SoftObjectPath.h"

AMCPSidebarPlayerController::AMCPSidebarPlayerController()
{
	ResolveSidebarWidgetClass();
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] PlayerController constructed. SidebarWidgetClass=%s"), *GetNameSafe(SidebarWidgetClass.Get()));
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

void AMCPSidebarPlayerController::ResolveSidebarWidgetClass()
{
	static const TCHAR* PreferredClassPath = TEXT("/Game/UI/Widget/WBP_MCPSidebar.WBP_MCPSidebar_C");
	static const TCHAR* PreferredAssetPath = TEXT("/Game/UI/Widget/WBP_MCPSidebar");

	UClass* LoadedClass = LoadClass<UMCPSidebarWidget>(nullptr, PreferredClassPath);
	if (LoadedClass == nullptr)
	{
		const FSoftClassPath SoftClassPath(PreferredClassPath);
		LoadedClass = SoftClassPath.TryLoadClass<UMCPSidebarWidget>();
	}

	if (LoadedClass == nullptr)
	{
		static ConstructorHelpers::FClassFinder<UMCPSidebarWidget> SidebarWidgetBPClass(PreferredAssetPath);
		if (SidebarWidgetBPClass.Succeeded())
		{
			LoadedClass = SidebarWidgetBPClass.Class;
		}
	}

	if (LoadedClass != nullptr)
	{
		SidebarWidgetClass = LoadedClass;
		UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Loaded sidebar widget blueprint class: %s"), *GetNameSafe(LoadedClass));
		return;
	}

	SidebarWidgetClass = UMCPSidebarWidget::StaticClass();
	UE_LOG(LogTemp, Warning, TEXT("[MCPSidebar] WBP_MCPSidebar not found at %s. Falling back to native UMCPSidebarWidget."), PreferredClassPath);
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
		ResolveSidebarWidgetClass();
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
