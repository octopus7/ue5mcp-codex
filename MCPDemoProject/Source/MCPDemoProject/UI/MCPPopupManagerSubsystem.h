#pragma once

#include "CoreMinimal.h"
#include "MCPBlurMessagePopupWidget.h"
#include "MCPConfirmPopupWidget.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MCPPopupManagerSubsystem.generated.h"

class APlayerController;
class UMCPBlurMessagePopupWidget;
class UMCPConfirmPopupWidget;
class UUserWidget;

UCLASS()
class MCPDEMOPROJECT_API UMCPPopupManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	bool RequestConfirmPopup(FMCPConfirmPopupRequest Request);
	bool RequestMessagePopup(FMCPMessagePopupRequest Request);
	bool IsConfirmPopupActive() const;
	bool IsMessagePopupActive() const;
	int32 GetPendingConfirmPopupCount() const;
	int32 GetPendingMessagePopupCount() const;

private:
	enum class EMCPQueuedPopupType : uint8
	{
		Confirm,
		Message
	};

	struct FMCPQueuedPopupRequest
	{
		EMCPQueuedPopupType Type = EMCPQueuedPopupType::Confirm;
		FMCPConfirmPopupRequest ConfirmRequest;
		FMCPMessagePopupRequest MessageRequest;
	};

	static constexpr int32 MaxPendingRequests = 32;

	void ResolveConfirmPopupClass();
	void ResolveMessagePopupClass();
	UMCPConfirmPopupWidget* GetOrCreateConfirmPopup();
	UMCPBlurMessagePopupWidget* GetOrCreateMessagePopup();
	APlayerController* ResolveOwningPlayerController() const;
	void SetPopupModalInput(bool bEnabled, UUserWidget* FocusWidget = nullptr);
	void TryShowNext();
	void HandlePopupConfirmed();
	void HandlePopupCancelled();
	void HandleMessagePopupClosed();
	void HandleActiveConfirmPopupResult(bool bConfirmed);
	void HandleActiveMessagePopupClosed();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPConfirmPopupWidget> ConfirmPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPBlurMessagePopupWidget> MessagePopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPConfirmPopupWidget> ConfirmPopupInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMCPBlurMessagePopupWidget> MessagePopupInstance;

	TArray<FMCPQueuedPopupRequest> PendingRequests;
	TOptional<FMCPQueuedPopupRequest> ActiveRequest;
	bool bPopupActive = false;
};

