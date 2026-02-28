#include "MCPPopupManagerSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPath.h"

void UMCPPopupManagerSubsystem::Deinitialize()
{
	if (ActiveConfirmPopupInstance != nullptr)
	{
		ActiveConfirmPopupInstance->OnConfirmed.RemoveAll(this);
		ActiveConfirmPopupInstance->OnCancelled.RemoveAll(this);
		ActiveConfirmPopupInstance->RemoveFromParent();
		ActiveConfirmPopupInstance = nullptr;
	}

	if (ActiveFormPopupInstance != nullptr)
	{
		ActiveFormPopupInstance->OnSubmitted.RemoveAll(this);
		ActiveFormPopupInstance->OnCancelled.RemoveAll(this);
		ActiveFormPopupInstance->RemoveFromParent();
		ActiveFormPopupInstance = nullptr;
	}

	PendingPopupOrder.Reset();
	PendingConfirmRequests.Reset();
	PendingFormRequests.Reset();
	ActivePopupType.Reset();
	ActiveConfirmRequest.Reset();
	ActiveFormRequest.Reset();
	bPopupActive = false;

	Super::Deinitialize();
}

bool UMCPPopupManagerSubsystem::RequestConfirmPopup(FMCPConfirmPopupRequest Request)
{
	if (PendingPopupOrder.Num() >= MaxPendingRequests)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MCPPopupManager] Confirm queue overflow (max=%d). Request rejected."), MaxPendingRequests);
		return false;
	}

	PendingConfirmRequests.Add(MoveTemp(Request));
	PendingPopupOrder.Add(EMCPPopupRequestType::Confirm);
	TryShowNext();
	return true;
}

bool UMCPPopupManagerSubsystem::RequestFormPopup(FMCPFormPopupRequest Request)
{
	if (PendingPopupOrder.Num() >= MaxPendingRequests)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MCPPopupManager] Form queue overflow (max=%d). Request rejected."), MaxPendingRequests);
		return false;
	}

	PendingFormRequests.Add(MoveTemp(Request));
	PendingPopupOrder.Add(EMCPPopupRequestType::Form);
	TryShowNext();
	return true;
}

bool UMCPPopupManagerSubsystem::IsConfirmPopupActive() const
{
	return bPopupActive && ActivePopupType.IsSet() && ActivePopupType.GetValue() == EMCPPopupRequestType::Confirm;
}

bool UMCPPopupManagerSubsystem::IsAnyPopupActive() const
{
	return bPopupActive;
}

int32 UMCPPopupManagerSubsystem::GetPendingConfirmPopupCount() const
{
	return PendingConfirmRequests.Num();
}

int32 UMCPPopupManagerSubsystem::GetPendingPopupCount() const
{
	return PendingPopupOrder.Num();
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
	if (ActiveConfirmPopupInstance != nullptr)
	{
		return ActiveConfirmPopupInstance;
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

	ActiveConfirmPopupInstance = CreateWidget<UMCPConfirmPopupWidget>(OwningPlayer, ConfirmPopupWidgetClass);
	if (ActiveConfirmPopupInstance != nullptr)
	{
		ActiveConfirmPopupInstance->OnConfirmed.RemoveAll(this);
		ActiveConfirmPopupInstance->OnCancelled.RemoveAll(this);
		ActiveConfirmPopupInstance->OnConfirmed.AddUObject(this, &UMCPPopupManagerSubsystem::HandlePopupConfirmed);
		ActiveConfirmPopupInstance->OnCancelled.AddUObject(this, &UMCPPopupManagerSubsystem::HandlePopupCancelled);
	}

	return ActiveConfirmPopupInstance;
}

void UMCPPopupManagerSubsystem::ResolveFormPopupClass()
{
	if (FormPopupWidgetClass != nullptr)
	{
		return;
	}

	static const TCHAR* PopupClassPath = TEXT("/Game/UI/Widget/WBP_MCPFormPopup.WBP_MCPFormPopup_C");
	UClass* LoadedClass = LoadClass<UMCPFormPopupWidget>(nullptr, PopupClassPath);
	if (LoadedClass == nullptr)
	{
		const FSoftClassPath SoftClassPath(PopupClassPath);
		LoadedClass = SoftClassPath.TryLoadClass<UMCPFormPopupWidget>();
	}

	if (LoadedClass != nullptr)
	{
		FormPopupWidgetClass = LoadedClass;
		UE_LOG(LogTemp, Log, TEXT("[MCPPopupManager] Loaded form popup widget class: %s"), *GetNameSafe(LoadedClass));
	}
	else
	{
		FormPopupWidgetClass = UMCPFormPopupWidget::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("[MCPPopupManager] Form popup widget class not found at %s. Falling back to native class."), PopupClassPath);
	}
}

UMCPFormPopupWidget* UMCPPopupManagerSubsystem::GetOrCreateFormPopup()
{
	if (ActiveFormPopupInstance != nullptr)
	{
		return ActiveFormPopupInstance;
	}

	ResolveFormPopupClass();
	if (FormPopupWidgetClass == nullptr)
	{
		return nullptr;
	}

	APlayerController* OwningPlayer = ResolveOwningPlayerController();
	if (OwningPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPPopupManager] Missing owning player controller while creating form popup."));
		return nullptr;
	}

	ActiveFormPopupInstance = CreateWidget<UMCPFormPopupWidget>(OwningPlayer, FormPopupWidgetClass);
	if (ActiveFormPopupInstance != nullptr)
	{
		ActiveFormPopupInstance->OnSubmitted.RemoveAll(this);
		ActiveFormPopupInstance->OnCancelled.RemoveAll(this);
		ActiveFormPopupInstance->OnSubmitted.AddUObject(this, &UMCPPopupManagerSubsystem::HandleFormPopupSubmitted);
		ActiveFormPopupInstance->OnCancelled.AddUObject(this, &UMCPPopupManagerSubsystem::HandleFormPopupCancelled);
	}

	return ActiveFormPopupInstance;
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
	if (bPopupActive)
	{
		return;
	}

	while (!PendingPopupOrder.IsEmpty())
	{
		const EMCPPopupRequestType NextType = PendingPopupOrder[0];
		PendingPopupOrder.RemoveAt(0);

		if (NextType == EMCPPopupRequestType::Confirm)
		{
			if (PendingConfirmRequests.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("[MCPPopupManager] Confirm popup order entry had no matching request."));
				continue;
			}

			UMCPConfirmPopupWidget* PopupWidget = GetOrCreateConfirmPopup();
			if (PopupWidget == nullptr)
			{
				UE_LOG(LogTemp, Error, TEXT("[MCPPopupManager] Failed to show confirm popup: widget creation failed."));
				continue;
			}

			ActiveConfirmRequest = MoveTemp(PendingConfirmRequests[0]);
			PendingConfirmRequests.RemoveAt(0);
			ActivePopupType = EMCPPopupRequestType::Confirm;
			bPopupActive = true;

			if (!PopupWidget->IsInViewport())
			{
				PopupWidget->AddToViewport(1100);
			}

			PopupWidget->OpenPopup(ActiveConfirmRequest.GetValue());
			SetPopupModalInput(true, PopupWidget);
			return;
		}

		if (PendingFormRequests.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[MCPPopupManager] Form popup order entry had no matching request."));
			continue;
		}

		UMCPFormPopupWidget* PopupWidget = GetOrCreateFormPopup();
		if (PopupWidget == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("[MCPPopupManager] Failed to show form popup: widget creation failed."));
			continue;
		}

		ActiveFormRequest = MoveTemp(PendingFormRequests[0]);
		PendingFormRequests.RemoveAt(0);
		ActivePopupType = EMCPPopupRequestType::Form;
		bPopupActive = true;

		if (!PopupWidget->IsInViewport())
		{
			PopupWidget->AddToViewport(1100);
		}

		PopupWidget->OpenPopup(ActiveFormRequest.GetValue());
		SetPopupModalInput(true, PopupWidget);
		return;
	}
}

void UMCPPopupManagerSubsystem::HandlePopupConfirmed()
{
	HandleActiveConfirmPopupResult(true);
}

void UMCPPopupManagerSubsystem::HandlePopupCancelled()
{
	HandleActiveConfirmPopupResult(false);
}

void UMCPPopupManagerSubsystem::HandleActiveConfirmPopupResult(bool bConfirmed)
{
	FMCPConfirmPopupRequest FinishedRequest;
	const bool bHadActiveRequest = ActiveConfirmRequest.IsSet();
	if (bHadActiveRequest)
	{
		FinishedRequest = MoveTemp(ActiveConfirmRequest.GetValue());
		ActiveConfirmRequest.Reset();
	}

	ActivePopupType.Reset();
	bPopupActive = false;
	SetPopupModalInput(false, nullptr);

	if (bHadActiveRequest && FinishedRequest.Callback.IsBound())
	{
		FinishedRequest.Callback.Execute(bConfirmed);
	}

	TryShowNext();
}

void UMCPPopupManagerSubsystem::HandleFormPopupSubmitted(const FMCPFormPopupResult& Result)
{
	HandleActiveFormPopupResult(Result);
}

void UMCPPopupManagerSubsystem::HandleFormPopupCancelled(const FMCPFormPopupResult& Result)
{
	HandleActiveFormPopupResult(Result);
}

void UMCPPopupManagerSubsystem::HandleActiveFormPopupResult(const FMCPFormPopupResult& Result)
{
	FMCPFormPopupRequest FinishedRequest;
	const bool bHadActiveRequest = ActiveFormRequest.IsSet();
	if (bHadActiveRequest)
	{
		FinishedRequest = MoveTemp(ActiveFormRequest.GetValue());
		ActiveFormRequest.Reset();
	}

	ActivePopupType.Reset();
	bPopupActive = false;
	SetPopupModalInput(false, nullptr);

	if (bHadActiveRequest && FinishedRequest.Callback.IsBound())
	{
		FinishedRequest.Callback.Execute(Result);
	}

	TryShowNext();
}
