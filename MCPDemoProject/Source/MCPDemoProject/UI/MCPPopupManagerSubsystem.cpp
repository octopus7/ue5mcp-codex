#include "MCPPopupManagerSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPath.h"

void UMCPPopupManagerSubsystem::Deinitialize()
{
	if (ConfirmPopupInstance != nullptr)
	{
		ConfirmPopupInstance->OnConfirmed.RemoveAll(this);
		ConfirmPopupInstance->OnCancelled.RemoveAll(this);
		ConfirmPopupInstance->RemoveFromParent();
		ConfirmPopupInstance = nullptr;
	}

	if (MessagePopupInstance != nullptr)
	{
		MessagePopupInstance->OnClosed.RemoveAll(this);
		MessagePopupInstance->RemoveFromParent();
		MessagePopupInstance = nullptr;
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
		UE_LOG(LogTemp, Warning, TEXT("[MCPPopupManager] Popup queue overflow (max=%d). Confirm request rejected."), MaxPendingRequests);
		return false;
	}

	FMCPQueuedPopupRequest QueuedRequest;
	QueuedRequest.Type = EMCPQueuedPopupType::Confirm;
	QueuedRequest.ConfirmRequest = MoveTemp(Request);
	PendingRequests.Add(MoveTemp(QueuedRequest));
	TryShowNext();
	return true;
}

bool UMCPPopupManagerSubsystem::RequestMessagePopup(FMCPMessagePopupRequest Request)
{
	if (PendingRequests.Num() >= MaxPendingRequests)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MCPPopupManager] Popup queue overflow (max=%d). Message request rejected."), MaxPendingRequests);
		return false;
	}

	FMCPQueuedPopupRequest QueuedRequest;
	QueuedRequest.Type = EMCPQueuedPopupType::Message;
	QueuedRequest.MessageRequest = MoveTemp(Request);
	PendingRequests.Add(MoveTemp(QueuedRequest));
	TryShowNext();
	return true;
}

bool UMCPPopupManagerSubsystem::IsConfirmPopupActive() const
{
	return bPopupActive && ActiveRequest.IsSet() && ActiveRequest.GetValue().Type == EMCPQueuedPopupType::Confirm;
}

bool UMCPPopupManagerSubsystem::IsMessagePopupActive() const
{
	return bPopupActive && ActiveRequest.IsSet() && ActiveRequest.GetValue().Type == EMCPQueuedPopupType::Message;
}

int32 UMCPPopupManagerSubsystem::GetPendingConfirmPopupCount() const
{
	int32 PendingCount = 0;
	for (const FMCPQueuedPopupRequest& PendingRequest : PendingRequests)
	{
		if (PendingRequest.Type == EMCPQueuedPopupType::Confirm)
		{
			PendingCount += 1;
		}
	}
	return PendingCount;
}

int32 UMCPPopupManagerSubsystem::GetPendingMessagePopupCount() const
{
	int32 PendingCount = 0;
	for (const FMCPQueuedPopupRequest& PendingRequest : PendingRequests)
	{
		if (PendingRequest.Type == EMCPQueuedPopupType::Message)
		{
			PendingCount += 1;
		}
	}
	return PendingCount;
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

void UMCPPopupManagerSubsystem::ResolveMessagePopupClass()
{
	if (MessagePopupWidgetClass != nullptr)
	{
		return;
	}

	static const TCHAR* PopupClassPath = TEXT("/Game/UI/Widget/WBP_MCPBlurMessagePopup.WBP_MCPBlurMessagePopup_C");
	UClass* LoadedClass = LoadClass<UMCPBlurMessagePopupWidget>(nullptr, PopupClassPath);
	if (LoadedClass == nullptr)
	{
		const FSoftClassPath SoftClassPath(PopupClassPath);
		LoadedClass = SoftClassPath.TryLoadClass<UMCPBlurMessagePopupWidget>();
	}

	if (LoadedClass != nullptr)
	{
		MessagePopupWidgetClass = LoadedClass;
		UE_LOG(LogTemp, Log, TEXT("[MCPPopupManager] Loaded blur message popup widget class: %s"), *GetNameSafe(LoadedClass));
	}
	else
	{
		MessagePopupWidgetClass = UMCPBlurMessagePopupWidget::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("[MCPPopupManager] Blur message popup widget class not found at %s. Falling back to native class."), PopupClassPath);
	}
}

UMCPConfirmPopupWidget* UMCPPopupManagerSubsystem::GetOrCreateConfirmPopup()
{
	if (ConfirmPopupInstance != nullptr)
	{
		return ConfirmPopupInstance;
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

	ConfirmPopupInstance = CreateWidget<UMCPConfirmPopupWidget>(OwningPlayer, ConfirmPopupWidgetClass);
	if (ConfirmPopupInstance != nullptr)
	{
		ConfirmPopupInstance->OnConfirmed.RemoveAll(this);
		ConfirmPopupInstance->OnCancelled.RemoveAll(this);
		ConfirmPopupInstance->OnConfirmed.AddUObject(this, &UMCPPopupManagerSubsystem::HandlePopupConfirmed);
		ConfirmPopupInstance->OnCancelled.AddUObject(this, &UMCPPopupManagerSubsystem::HandlePopupCancelled);
	}

	return ConfirmPopupInstance;
}

UMCPBlurMessagePopupWidget* UMCPPopupManagerSubsystem::GetOrCreateMessagePopup()
{
	if (MessagePopupInstance != nullptr)
	{
		return MessagePopupInstance;
	}

	ResolveMessagePopupClass();
	if (MessagePopupWidgetClass == nullptr)
	{
		return nullptr;
	}

	APlayerController* OwningPlayer = ResolveOwningPlayerController();
	if (OwningPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPPopupManager] Missing owning player controller while creating blur message popup."));
		return nullptr;
	}

	MessagePopupInstance = CreateWidget<UMCPBlurMessagePopupWidget>(OwningPlayer, MessagePopupWidgetClass);
	if (MessagePopupInstance != nullptr)
	{
		MessagePopupInstance->OnClosed.RemoveAll(this);
		MessagePopupInstance->OnClosed.AddUObject(this, &UMCPPopupManagerSubsystem::HandleMessagePopupClosed);
	}

	return MessagePopupInstance;
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

	const EMCPQueuedPopupType NextRequestType = PendingRequests[0].Type;

	if (NextRequestType == EMCPQueuedPopupType::Confirm)
	{
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

		PopupWidget->OpenPopup(ActiveRequest.GetValue().ConfirmRequest);
		SetPopupModalInput(true, PopupWidget);
		return;
	}

	UMCPBlurMessagePopupWidget* PopupWidget = GetOrCreateMessagePopup();
	if (PopupWidget == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[MCPPopupManager] Failed to show blur message popup: widget creation failed."));
		return;
	}

	if (!PopupWidget->IsInViewport())
	{
		PopupWidget->AddToViewport(1100);
	}

	ActiveRequest = MoveTemp(PendingRequests[0]);
	PendingRequests.RemoveAt(0);
	bPopupActive = true;

	PopupWidget->OpenPopup(ActiveRequest.GetValue().MessageRequest);
	SetPopupModalInput(true, PopupWidget);
}

void UMCPPopupManagerSubsystem::HandlePopupConfirmed()
{
	HandleActiveConfirmPopupResult(true);
}

void UMCPPopupManagerSubsystem::HandlePopupCancelled()
{
	HandleActiveConfirmPopupResult(false);
}

void UMCPPopupManagerSubsystem::HandleMessagePopupClosed()
{
	HandleActiveMessagePopupClosed();
}

void UMCPPopupManagerSubsystem::HandleActiveConfirmPopupResult(bool bConfirmed)
{
	if (!ActiveRequest.IsSet() || ActiveRequest.GetValue().Type != EMCPQueuedPopupType::Confirm)
	{
		return;
	}

	FMCPConfirmPopupRequest FinishedRequest = MoveTemp(ActiveRequest.GetValue().ConfirmRequest);
	ActiveRequest.Reset();

	bPopupActive = false;
	SetPopupModalInput(false, nullptr);

	if (FinishedRequest.Callback.IsBound())
	{
		FinishedRequest.Callback.Execute(bConfirmed);
	}

	TryShowNext();
}

void UMCPPopupManagerSubsystem::HandleActiveMessagePopupClosed()
{
	if (!ActiveRequest.IsSet() || ActiveRequest.GetValue().Type != EMCPQueuedPopupType::Message)
	{
		return;
	}

	FMCPMessagePopupRequest FinishedRequest = MoveTemp(ActiveRequest.GetValue().MessageRequest);
	ActiveRequest.Reset();

	bPopupActive = false;
	SetPopupModalInput(false, nullptr);

	if (FinishedRequest.Callback.IsBound())
	{
		FinishedRequest.Callback.Execute();
	}

	TryShowNext();
}

