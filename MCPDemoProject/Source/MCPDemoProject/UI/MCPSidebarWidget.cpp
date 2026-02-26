#include "MCPSidebarWidget.h"

#include "MCPABValuePopupWidget.h"
#include "MCPMessagePopupWidget.h"
#include "MCPScrollGridPopupWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPath.h"

void UMCPSidebarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] NativeConstruct begin."));

	ResolveMessagePopupClass();
	ResolveABValuePopupClass();
	ResolveScrollGridPopupClass();

	DisplayedA = Menu2InitialA;
	DisplayedB = Menu2InitialB;
	DisplayedC = Menu2InitialC;
	RefreshABValueDisplay();

	if (MCP_Menu1Button != nullptr)
	{
		MCP_Menu1Button->OnClicked.RemoveDynamic(this, &UMCPSidebarWidget::HandleMenu1Clicked);
		MCP_Menu1Button->OnClicked.AddDynamic(this, &UMCPSidebarWidget::HandleMenu1Clicked);
	}

	if (MCP_Menu2Button != nullptr)
	{
		MCP_Menu2Button->OnClicked.RemoveDynamic(this, &UMCPSidebarWidget::HandleMenu2Clicked);
		MCP_Menu2Button->OnClicked.AddDynamic(this, &UMCPSidebarWidget::HandleMenu2Clicked);
	}

	if (MCP_Menu3Button != nullptr)
	{
		MCP_Menu3Button->OnClicked.RemoveDynamic(this, &UMCPSidebarWidget::HandleMenu3Clicked);
		MCP_Menu3Button->OnClicked.AddDynamic(this, &UMCPSidebarWidget::HandleMenu3Clicked);
	}

	if (MCP_Menu4Button != nullptr)
	{
		MCP_Menu4Button->OnClicked.RemoveDynamic(this, &UMCPSidebarWidget::HandleMenu4Clicked);
		MCP_Menu4Button->OnClicked.AddDynamic(this, &UMCPSidebarWidget::HandleMenu4Clicked);
	}

	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] NativeConstruct end. Panel=%s Buttons: M1=%s M2=%s M3=%s M4=%s"),
		MCP_SidebarPanel != nullptr ? TEXT("true") : TEXT("false"),
		MCP_Menu1Button != nullptr ? TEXT("true") : TEXT("false"),
		MCP_Menu2Button != nullptr ? TEXT("true") : TEXT("false"),
		MCP_Menu3Button != nullptr ? TEXT("true") : TEXT("false"),
		MCP_Menu4Button != nullptr ? TEXT("true") : TEXT("false"));
}

void UMCPSidebarWidget::HandleMenu1Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Menu1 clicked."));

	UMCPMessagePopupWidget* PopupWidget = GetOrCreateMessagePopup();
	if (PopupWidget == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] Failed to open message popup: widget creation failed."));
		return;
	}

	if (!PopupWidget->IsInViewport())
	{
		PopupWidget->AddToViewport(1000);
	}

	PopupWidget->OpenPopup(
		FText::FromString(TEXT("Message Box")),
		FText::FromString(TEXT("This is a simple message popup.")));
	SetPopupModalInput(true, PopupWidget);
}

void UMCPSidebarWidget::HandleMenu2Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Menu2 clicked."));

	UMCPABValuePopupWidget* PopupWidget = GetOrCreateABValuePopup();
	if (PopupWidget == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] Failed to open AB popup: widget creation failed."));
		return;
	}

	if (!PopupWidget->IsInViewport())
	{
		PopupWidget->AddToViewport(1000);
	}

	PopupWidget->OpenPopup(DisplayedA, DisplayedB, DisplayedC);
	SetPopupModalInput(true, PopupWidget);
}

void UMCPSidebarWidget::HandleMenu3Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Menu3 clicked."));

	UMCPScrollGridPopupWidget* PopupWidget = GetOrCreateScrollGridPopup();
	if (PopupWidget == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] Failed to open scroll-grid popup: widget creation failed."));
		return;
	}

	if (!PopupWidget->IsInViewport())
	{
		PopupWidget->AddToViewport(1000);
	}

	PopupWidget->OpenPopup();
	SetPopupModalInput(true, PopupWidget);
}

void UMCPSidebarWidget::HandleMenu4Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Menu4 (TileView) clicked."));
	ShowDebugMessage(TEXT("TileView clicked"), FColor::Silver);
}

void UMCPSidebarWidget::ResolveMessagePopupClass()
{
	if (MessagePopupWidgetClass != nullptr)
	{
		return;
	}

	static const TCHAR* PopupClassPath = TEXT("/Game/UI/Widget/WBP_MCPMessagePopup.WBP_MCPMessagePopup_C");
	UClass* LoadedClass = LoadClass<UMCPMessagePopupWidget>(nullptr, PopupClassPath);
	if (LoadedClass == nullptr)
	{
		const FSoftClassPath SoftClassPath(PopupClassPath);
		LoadedClass = SoftClassPath.TryLoadClass<UMCPMessagePopupWidget>();
	}

	if (LoadedClass != nullptr)
	{
		MessagePopupWidgetClass = LoadedClass;
		UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Loaded popup widget class: %s"), *GetNameSafe(LoadedClass));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[MCPSidebar] Popup widget class not found at %s"), PopupClassPath);
	}
}

void UMCPSidebarWidget::ResolveABValuePopupClass()
{
	if (ABValuePopupWidgetClass != nullptr)
	{
		return;
	}

	static const TCHAR* PopupClassPath = TEXT("/Game/UI/Widget/WBP_MCPABValuePopup.WBP_MCPABValuePopup_C");
	UClass* LoadedClass = LoadClass<UMCPABValuePopupWidget>(nullptr, PopupClassPath);
	if (LoadedClass == nullptr)
	{
		const FSoftClassPath SoftClassPath(PopupClassPath);
		LoadedClass = SoftClassPath.TryLoadClass<UMCPABValuePopupWidget>();
	}

	if (LoadedClass != nullptr)
	{
		ABValuePopupWidgetClass = LoadedClass;
		UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Loaded AB popup widget class: %s"), *GetNameSafe(LoadedClass));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[MCPSidebar] AB popup widget class not found at %s"), PopupClassPath);
	}
}

void UMCPSidebarWidget::ResolveScrollGridPopupClass()
{
	if (ScrollGridPopupWidgetClass != nullptr)
	{
		return;
	}

	static const TCHAR* PopupClassPath = TEXT("/Game/UI/Widget/WBP_MCPScrollGridPopup.WBP_MCPScrollGridPopup_C");
	UClass* LoadedClass = LoadClass<UMCPScrollGridPopupWidget>(nullptr, PopupClassPath);
	if (LoadedClass == nullptr)
	{
		const FSoftClassPath SoftClassPath(PopupClassPath);
		LoadedClass = SoftClassPath.TryLoadClass<UMCPScrollGridPopupWidget>();
	}

	if (LoadedClass != nullptr)
	{
		ScrollGridPopupWidgetClass = LoadedClass;
		UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Loaded scroll-grid popup widget class: %s"), *GetNameSafe(LoadedClass));
	}
	else
	{
		ScrollGridPopupWidgetClass = UMCPScrollGridPopupWidget::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("[MCPSidebar] Scroll-grid popup widget class not found at %s. Falling back to native class."), PopupClassPath);
	}
}

UMCPMessagePopupWidget* UMCPSidebarWidget::GetOrCreateMessagePopup()
{
	if (MessagePopupWidgetInstance != nullptr)
	{
		return MessagePopupWidgetInstance;
	}

	ResolveMessagePopupClass();
	if (MessagePopupWidgetClass == nullptr)
	{
		return nullptr;
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (OwningPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] GetOwningPlayer returned null while creating popup."));
		return nullptr;
	}

	MessagePopupWidgetInstance = CreateWidget<UMCPMessagePopupWidget>(OwningPlayer, MessagePopupWidgetClass);
	if (MessagePopupWidgetInstance != nullptr)
	{
		MessagePopupWidgetInstance->OnPopupClosed.AddUObject(this, &UMCPSidebarWidget::HandleMessagePopupClosed);
	}

	return MessagePopupWidgetInstance;
}

UMCPABValuePopupWidget* UMCPSidebarWidget::GetOrCreateABValuePopup()
{
	if (ABValuePopupWidgetInstance != nullptr)
	{
		return ABValuePopupWidgetInstance;
	}

	ResolveABValuePopupClass();
	if (ABValuePopupWidgetClass == nullptr)
	{
		return nullptr;
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (OwningPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] GetOwningPlayer returned null while creating AB popup."));
		return nullptr;
	}

	ABValuePopupWidgetInstance = CreateWidget<UMCPABValuePopupWidget>(OwningPlayer, ABValuePopupWidgetClass);
	if (ABValuePopupWidgetInstance != nullptr)
	{
		ABValuePopupWidgetInstance->OnABPopupConfirmed.AddUObject(this, &UMCPSidebarWidget::HandleABPopupConfirmed);
		ABValuePopupWidgetInstance->OnABPopupCancelled.AddUObject(this, &UMCPSidebarWidget::HandleABPopupCancelled);
	}

	return ABValuePopupWidgetInstance;
}

UMCPScrollGridPopupWidget* UMCPSidebarWidget::GetOrCreateScrollGridPopup()
{
	if (ScrollGridPopupWidgetInstance != nullptr)
	{
		return ScrollGridPopupWidgetInstance;
	}

	ResolveScrollGridPopupClass();
	if (ScrollGridPopupWidgetClass == nullptr)
	{
		return nullptr;
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (OwningPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] GetOwningPlayer returned null while creating scroll-grid popup."));
		return nullptr;
	}

	ScrollGridPopupWidgetInstance = CreateWidget<UMCPScrollGridPopupWidget>(OwningPlayer, ScrollGridPopupWidgetClass);
	if (ScrollGridPopupWidgetInstance != nullptr)
	{
		ScrollGridPopupWidgetInstance->OnScrollGridPopupClosed.AddUObject(this, &UMCPSidebarWidget::HandleScrollGridPopupClosed);
	}

	return ScrollGridPopupWidgetInstance;
}

void UMCPSidebarWidget::SetPopupModalInput(bool bEnabled, UUserWidget* FocusWidget)
{
	APlayerController* OwningPlayer = GetOwningPlayer();
	if (OwningPlayer == nullptr)
	{
		return;
	}

	if (bEnabled)
	{
		FInputModeUIOnly InputMode;
		UUserWidget* WidgetToFocus = FocusWidget;
		if (WidgetToFocus == nullptr)
		{
			if (ScrollGridPopupWidgetInstance != nullptr && ScrollGridPopupWidgetInstance->IsInViewport())
			{
				WidgetToFocus = ScrollGridPopupWidgetInstance;
			}
			else if (ABValuePopupWidgetInstance != nullptr && ABValuePopupWidgetInstance->IsInViewport())
			{
				WidgetToFocus = ABValuePopupWidgetInstance;
			}
			else if (MessagePopupWidgetInstance != nullptr && MessagePopupWidgetInstance->IsInViewport())
			{
				WidgetToFocus = MessagePopupWidgetInstance;
			}
		}

		if (WidgetToFocus != nullptr)
		{
			InputMode.SetWidgetToFocus(WidgetToFocus->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		OwningPlayer->SetInputMode(InputMode);
		OwningPlayer->bShowMouseCursor = true;
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	OwningPlayer->SetInputMode(InputMode);
	OwningPlayer->bShowMouseCursor = true;
}

void UMCPSidebarWidget::HandleMessagePopupClosed()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Message popup closed by OK."));
	SetPopupModalInput(false, nullptr);
}

void UMCPSidebarWidget::HandleABPopupConfirmed(int32 FinalA, int32 FinalB, int32 FinalC)
{
	DisplayedA = FinalA;
	DisplayedB = FinalB;
	DisplayedC = FinalC;
	RefreshABValueDisplay();
	SetPopupModalInput(false, nullptr);

	const FString FinalValueLog = FString::Printf(TEXT("AB confirmed. A=%d B=%d C=%d"), DisplayedA, DisplayedB, DisplayedC);
	ShowDebugMessage(FinalValueLog, FColor::Green);
}

void UMCPSidebarWidget::HandleABPopupCancelled()
{
	SetPopupModalInput(false, nullptr);
	ShowDebugMessage(TEXT("AB popup canceled"), FColor::Orange);
}

void UMCPSidebarWidget::HandleScrollGridPopupClosed()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Scroll-grid popup closed."));
	SetPopupModalInput(false, nullptr);
}

void UMCPSidebarWidget::RefreshABValueDisplay()
{
	if (MCP_ABStatusLabel == nullptr)
	{
		return;
	}

	MCP_ABStatusLabel->SetText(FText::FromString(FString::Printf(TEXT("A: %d    B: %d    C: %d"), DisplayedA, DisplayedB, DisplayedC)));
}

void UMCPSidebarWidget::ShowDebugMessage(const FString& Message, const FColor& Color) const
{
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, Color, Message);
	}
}
