#include "MCPPopupManagerSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPath.h"

void UMCPPopupManagerSubsystem::Deinitialize()
{
	if (ActivePopupInstance != nullptr)
	{
		ActivePopupInstance->OnConfirmed.RemoveAll(this);
		ActivePopupInstance->OnCancelled.RemoveAll(this);
		ActivePopupInstance->RemoveFromParent();
		ActivePopupInstance = nullptr;
	}

	PendingRequests.Reset();
	ActiveRequest.Reset();
	bPopupActive = false;

	Super::Deinitialize();
}

bool UMCPPopupManagerSubsystem::RequestConfirmPopup(FMCPConfirmPopupRequest Request)
{
	if (PendingRequests.Num() >= MaxPendingRequests)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MCPPopupManager] Confirm queue overflow (max=%d). Request rejected."), MaxPendingRequests);
		return false;
	}

	PendingRequests.Add(MoveTemp(Request));
	TryShowNext();
	return true;
}

bool UMCPPopupManagerSubsystem::IsConfirmPopupActive() const
{
	return bPopupActive;
}

int32 UMCPPopupManagerSubsystem::GetPendingConfirmPopupCount() const
{
	return PendingRequests.Num();
}

void UMCPPopupManagerSubsystem::ResolveConfirmPopupClass()
{
	if (ConfirmPopupWidgetClass != nullptr)
	{
		return;
	}

	static const TCHAR* PopupClassPath = TEXT("/Game/UI/Widget/WBP_MCPConfirmPopup.WBP_MCPConfirmPopup_C");
	UClass* LoadedClass = LoadClass<UMCPConfirmPopupWidget>(nullptr, PopupClassPath);
	if (LoadedClass == nullptr)
	{
		const FSoftClassPath SoftClassPath(PopupClassPath);
		LoadedClass = SoftClassPath.TryLoadClass<UMCPConfirmPopupWidget>();
	}

	if (LoadedClass != nullptr)
	{
		ConfirmPopupWidgetClass = LoadedClass;
		UE_LOG(LogTemp, Log, TEXT("[MCPPopupManager] Loaded confirm popup widget class: %s"), *GetNameSafe(LoadedClass));
	}
	else
	{
		ConfirmPopupWidgetClass = UMCPConfirmPopupWidget::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("[MCPPopupManager] Confirm popup widget class not found at %s. Falling back to native class."), PopupClassPath);
	}
}

UMCPConfirmPopupWidget* UMCPPopupManagerSubsystem::GetOrCreateConfirmPopup()
{
	if (ActivePopupInstance != nullptr)
	{
		return ActivePopupInstance;
	}

	ResolveConfirmPopupClass();
	if (ConfirmPopupWidgetClass == nullptr)
	{
		return nullptr;
	}

	APlayerController* OwningPlayer = ResolveOwningPlayerController();
	if (OwningPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPPopupManager] Missing owning player controller while creating confirm popup."));
		return nullptr;
	}

	ActivePopupInstance = CreateWidget<UMCPConfirmPopupWidget>(OwningPlayer, ConfirmPopupWidgetClass);
	if (ActivePopupInstance != nullptr)
	{
		ActivePopupInstance->OnConfirmed.RemoveAll(this);
		ActivePopupInstance->OnCancelled.RemoveAll(this);
		ActivePopupInstance->OnConfirmed.AddUObject(this, &UMCPPopupManagerSubsystem::HandlePopupConfirmed);
		ActivePopupInstance->OnCancelled.AddUObject(this, &UMCPPopupManagerSubsystem::HandlePopupCancelled);
	}

	return ActivePopupInstance;
}

APlayerController* UMCPPopupManagerSubsystem::ResolveOwningPlayerController() const
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (LocalPlayer == nullptr)
	{
		return nullptr;
	}

	if (LocalPlayer->PlayerController != nullptr)
	{
		return LocalPlayer->PlayerController;
	}

	return LocalPlayer->GetPlayerController(GetWorld());
}

void UMCPPopupManagerSubsystem::SetPopupModalInput(bool bEnabled, UUserWidget* FocusWidget)
{
	APlayerController* OwningPlayer = ResolveOwningPlayerController();
	if (OwningPlayer == nullptr)
	{
		return;
	}

	if (bEnabled)
	{
		FInputModeUIOnly InputMode;
		if (FocusWidget != nullptr)
		{
			InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
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

void UMCPPopupManagerSubsystem::TryShowNext()
{
	if (bPopupActive || PendingRequests.IsEmpty())
	{
		return;
	}

	UMCPConfirmPopupWidget* PopupWidget = GetOrCreateConfirmPopup();
	if (PopupWidget == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPPopupManager] Failed to show confirm popup: widget creation failed."));
		return;
	}

	ActiveRequest = MoveTemp(PendingRequests[0]);
	PendingRequests.RemoveAt(0);
	bPopupActive = true;

	if (!PopupWidget->IsInViewport())
	{
		PopupWidget->AddToViewport(1100);
	}

	PopupWidget->OpenPopup(ActiveRequest.GetValue());
	SetPopupModalInput(true, PopupWidget);
}

void UMCPPopupManagerSubsystem::HandlePopupConfirmed()
{
	HandleActivePopupResult(true);
}

void UMCPPopupManagerSubsystem::HandlePopupCancelled()
{
	HandleActivePopupResult(false);
}

void UMCPPopupManagerSubsystem::HandleActivePopupResult(bool bConfirmed)
{
	FMCPConfirmPopupRequest FinishedRequest;
	const bool bHadActiveRequest = ActiveRequest.IsSet();
	if (bHadActiveRequest)
	{
		FinishedRequest = MoveTemp(ActiveRequest.GetValue());
		ActiveRequest.Reset();
	}

	bPopupActive = false;
	SetPopupModalInput(false, nullptr);

	if (bHadActiveRequest && FinishedRequest.Callback.IsBound())
	{
		FinishedRequest.Callback.Execute(bConfirmed);
	}

	TryShowNext();
}

