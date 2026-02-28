#include "MCPSidebarWidget.h"

#include "MCPABValuePopupWidget.h"
#include "MCPMessagePopupWidget.h"
#include "MCPPopupManagerSubsystem.h"
#include "MCPScrollGridPopupWidget.h"
#include "MCPScrollTilePopupWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPath.h"

void UMCPSidebarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] NativeConstruct begin."));

	ResolveMessagePopupClass();
	ResolveABValuePopupClass();
	ResolveScrollGridPopupClass();
	ResolveScrollTilePopupClass();

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

	if (MCP_Menu5Button != nullptr)
	{
		MCP_Menu5Button->OnClicked.RemoveDynamic(this, &UMCPSidebarWidget::HandleMenu5Clicked);
		MCP_Menu5Button->OnClicked.AddDynamic(this, &UMCPSidebarWidget::HandleMenu5Clicked);
	}

	if (MCP_Menu6Button != nullptr)
	{
		MCP_Menu6Button->OnClicked.RemoveDynamic(this, &UMCPSidebarWidget::HandleMenu6Clicked);
		MCP_Menu6Button->OnClicked.AddDynamic(this, &UMCPSidebarWidget::HandleMenu6Clicked);
	}

	if (MCP_Menu7Button != nullptr)
	{
		MCP_Menu7Button->OnClicked.RemoveDynamic(this, &UMCPSidebarWidget::HandleMenu7Clicked);
		MCP_Menu7Button->OnClicked.AddDynamic(this, &UMCPSidebarWidget::HandleMenu7Clicked);
	}

	if (MCP_Menu5Label != nullptr && MCP_Menu5Label->GetText().IsEmpty())
	{
		MCP_Menu5Label->SetText(FText::FromString(TEXT("Confirm Popup")));
	}

	if (MCP_Menu6Label != nullptr && MCP_Menu6Label->GetText().IsEmpty())
	{
		MCP_Menu6Label->SetText(FText::FromString(TEXT("Form Popup")));
	}

	if (MCP_Menu7Label != nullptr && MCP_Menu7Label->GetText().IsEmpty())
	{
		MCP_Menu7Label->SetText(FText::FromString(TEXT("Blur Message Popup")));
	}

	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] NativeConstruct end. Panel=%s Buttons: M1=%s M2=%s M3=%s M4=%s M5=%s M6=%s M7=%s"),
		MCP_SidebarPanel != nullptr ? TEXT("true") : TEXT("false"),
		MCP_Menu1Button != nullptr ? TEXT("true") : TEXT("false"),
		MCP_Menu2Button != nullptr ? TEXT("true") : TEXT("false"),
		MCP_Menu3Button != nullptr ? TEXT("true") : TEXT("false"),
		MCP_Menu4Button != nullptr ? TEXT("true") : TEXT("false"),
		MCP_Menu5Button != nullptr ? TEXT("true") : TEXT("false"),
		MCP_Menu6Button != nullptr ? TEXT("true") : TEXT("false"),
		MCP_Menu7Button != nullptr ? TEXT("true") : TEXT("false"));
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

	UMCPScrollTilePopupWidget* PopupWidget = GetOrCreateScrollTilePopup();
	if (PopupWidget == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] Failed to open scroll-tile popup: widget creation failed."));
		return;
	}

	if (!PopupWidget->IsInViewport())
	{
		PopupWidget->AddToViewport(1000);
	}

	PopupWidget->OpenPopup();
	SetPopupModalInput(true, PopupWidget);
}

void UMCPSidebarWidget::HandleMenu5Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Menu5 (ConfirmPopup) clicked."));

	ULocalPlayer* OwningLocalPlayer = GetOwningLocalPlayer();
	if (OwningLocalPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] Failed to open confirm popup: owning local player is null."));
		return;
	}

	UMCPPopupManagerSubsystem* PopupManager = OwningLocalPlayer->GetSubsystem<UMCPPopupManagerSubsystem>();
	if (PopupManager == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] Failed to open confirm popup: popup manager subsystem is null."));
		return;
	}

	FMCPConfirmPopupRequest Request;
	Request.Title = FText::FromString(TEXT("Confirm Popup"));
	Request.Message = FText::FromString(TEXT("Proceed with this test action?"));
	Request.ConfirmLabel = FText::FromString(TEXT("Confirm"));
	Request.CancelLabel = FText::FromString(TEXT("Cancel"));
	Request.Callback = FMCPConfirmPopupResultCallback::CreateUObject(this, &UMCPSidebarWidget::HandleMenu5ConfirmResult);

	if (!PopupManager->RequestConfirmPopup(MoveTemp(Request)))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MCPSidebar] Confirm popup request rejected by popup manager."));
		ShowDebugMessage(TEXT("Confirm popup request rejected"), FColor::Red);
	}
}

void UMCPSidebarWidget::HandleMenu6Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Menu6 (FormPopup) clicked."));

	ULocalPlayer* OwningLocalPlayer = GetOwningLocalPlayer();
	if (OwningLocalPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] Failed to open form popup: owning local player is null."));
		return;
	}

	UMCPPopupManagerSubsystem* PopupManager = OwningLocalPlayer->GetSubsystem<UMCPPopupManagerSubsystem>();
	if (PopupManager == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] Failed to open form popup: popup manager subsystem is null."));
		return;
	}

	FMCPFormPopupRequest Request;
	Request.Title = FText::FromString(TEXT("Form Sample Popup"));
	Request.Message = FText::FromString(TEXT("Fill the form and submit."));
	Request.NameHint = FText::FromString(TEXT("Enter name"));
	Request.CheckLabel = FText::FromString(TEXT("Enable Option"));
	Request.ComboOptions = { TEXT("Option A"), TEXT("Option B"), TEXT("Option C") };
	Request.DefaultComboIndex = 0;
	Request.SubmitLabel = FText::FromString(TEXT("Submit"));
	Request.CancelLabel = FText::FromString(TEXT("Cancel"));
	Request.Callback = FMCPFormPopupResultCallback::CreateUObject(this, &UMCPSidebarWidget::HandleMenu6FormResult);

	if (!PopupManager->RequestFormPopup(MoveTemp(Request)))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MCPSidebar] Form popup request rejected by popup manager."));
		ShowDebugMessage(TEXT("Form popup request rejected"), FColor::Red);
	}
}

void UMCPSidebarWidget::HandleMenu7Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Menu7 (BlurMessagePopup) clicked."));

	ULocalPlayer* OwningLocalPlayer = GetOwningLocalPlayer();
	if (OwningLocalPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] Failed to open blur message popup: owning local player is null."));
		return;
	}

	UMCPPopupManagerSubsystem* PopupManager = OwningLocalPlayer->GetSubsystem<UMCPPopupManagerSubsystem>();
	if (PopupManager == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] Failed to open blur message popup: popup manager subsystem is null."));
		return;
	}

	FMCPMessagePopupRequest Request;
	Request.Title = FText::FromString(TEXT("Blur Message Popup"));
	Request.Message = FText::FromString(TEXT("This popup uses rounded background blur and dark message panel."));
	Request.OkLabel = FText::FromString(TEXT("OK"));
	Request.Callback = FMCPMessagePopupClosedCallback::CreateUObject(this, &UMCPSidebarWidget::HandleMenu7MessageClosed);

	if (!PopupManager->RequestMessagePopup(MoveTemp(Request)))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MCPSidebar] Blur message popup request rejected by popup manager."));
		ShowDebugMessage(TEXT("Blur message popup request rejected"), FColor::Red);
	}
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

void UMCPSidebarWidget::ResolveScrollTilePopupClass()
{
	if (ScrollTilePopupWidgetClass != nullptr)
	{
		return;
	}

	static const TCHAR* PopupClassPath = TEXT("/Game/UI/Widget/WBP_MCPScrollTilePopup.WBP_MCPScrollTilePopup_C");
	UClass* LoadedClass = LoadClass<UMCPScrollTilePopupWidget>(nullptr, PopupClassPath);
	if (LoadedClass == nullptr)
	{
		const FSoftClassPath SoftClassPath(PopupClassPath);
		LoadedClass = SoftClassPath.TryLoadClass<UMCPScrollTilePopupWidget>();
	}

	if (LoadedClass != nullptr)
	{
		ScrollTilePopupWidgetClass = LoadedClass;
		UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Loaded scroll-tile popup widget class: %s"), *GetNameSafe(LoadedClass));
	}
	else
	{
		ScrollTilePopupWidgetClass = UMCPScrollTilePopupWidget::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("[MCPSidebar] Scroll-tile popup widget class not found at %s. Falling back to native class."), PopupClassPath);
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

UMCPScrollTilePopupWidget* UMCPSidebarWidget::GetOrCreateScrollTilePopup()
{
	if (ScrollTilePopupWidgetInstance != nullptr)
	{
		return ScrollTilePopupWidgetInstance;
	}

	ResolveScrollTilePopupClass();
	if (ScrollTilePopupWidgetClass == nullptr)
	{
		return nullptr;
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (OwningPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPSidebar] GetOwningPlayer returned null while creating scroll-tile popup."));
		return nullptr;
	}

	ScrollTilePopupWidgetInstance = CreateWidget<UMCPScrollTilePopupWidget>(OwningPlayer, ScrollTilePopupWidgetClass);
	if (ScrollTilePopupWidgetInstance != nullptr)
	{
		ScrollTilePopupWidgetInstance->OnScrollTilePopupClosed.AddUObject(this, &UMCPSidebarWidget::HandleScrollTilePopupClosed);
	}

	return ScrollTilePopupWidgetInstance;
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
			if (ScrollTilePopupWidgetInstance != nullptr && ScrollTilePopupWidgetInstance->IsInViewport())
			{
				WidgetToFocus = ScrollTilePopupWidgetInstance;
			}
			else if (ScrollGridPopupWidgetInstance != nullptr && ScrollGridPopupWidgetInstance->IsInViewport())
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

void UMCPSidebarWidget::HandleScrollTilePopupClosed()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Scroll-tile popup closed."));
	SetPopupModalInput(false, nullptr);
}

void UMCPSidebarWidget::HandleMenu5ConfirmResult(bool bConfirmed)
{
	ShowDebugMessage(
		bConfirmed ? TEXT("Confirm popup confirmed") : TEXT("Confirm popup canceled"),
		bConfirmed ? FColor::Green : FColor::Orange);
}

void UMCPSidebarWidget::HandleMenu6FormResult(const FMCPFormPopupResult& Result)
{
	const FString Status = Result.bSubmitted ? TEXT("submitted") : TEXT("canceled");
	const FString Message = FString::Printf(
		TEXT("Form popup %s: Name='%s' Checked=%s Option='%s'"),
		*Status,
		*Result.NameValue.ToString(),
		Result.bChecked ? TEXT("true") : TEXT("false"),
		*Result.SelectedOption);

	ShowDebugMessage(Message, Result.bSubmitted ? FColor::Green : FColor::Orange);
}

void UMCPSidebarWidget::HandleMenu7MessageClosed()
{
	ShowDebugMessage(TEXT("Blur message popup closed"), FColor::Green);
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
