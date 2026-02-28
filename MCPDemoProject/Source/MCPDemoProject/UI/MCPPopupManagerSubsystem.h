#pragma once

#include "CoreMinimal.h"
#include "MCPBlurMessagePopupWidget.h"
#include "MCPConfirmPopupWidget.h"
#include "MCPFormPopupWidget.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MCPPopupManagerSubsystem.generated.h"

class APlayerController;
class UMCPBlurMessagePopupWidget;
class UMCPConfirmPopupWidget;
class UMCPFormPopupWidget;
class UUserWidget;

UCLASS()
class MCPDEMOPROJECT_API UMCPPopupManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	bool RequestConfirmPopup(FMCPConfirmPopupRequest Request);
	bool RequestFormPopup(FMCPFormPopupRequest Request);
	bool RequestMessagePopup(FMCPMessagePopupRequest Request);
	bool IsConfirmPopupActive() const;
	bool IsAnyPopupActive() const;
	bool IsMessagePopupActive() const;
	int32 GetPendingConfirmPopupCount() const;
	int32 GetPendingPopupCount() const;
	int32 GetPendingMessagePopupCount() const;

private:
	enum class EMCPPopupRequestType : uint8
	{
		Confirm,
		Form,
		Message
	};

	struct FMCPQueuedPopupRequest
	{
		EMCPPopupRequestType Type = EMCPPopupRequestType::Confirm;
		FMCPConfirmPopupRequest ConfirmRequest;
		FMCPFormPopupRequest FormRequest;
		FMCPMessagePopupRequest MessageRequest;
	};

	static constexpr int32 MaxPendingRequests = 32;

	void ResolveConfirmPopupClass();
	UMCPConfirmPopupWidget* GetOrCreateConfirmPopup();
	void ResolveFormPopupClass();
	UMCPFormPopupWidget* GetOrCreateFormPopup();
	void ResolveMessagePopupClass();
	UMCPBlurMessagePopupWidget* GetOrCreateMessagePopup();
	APlayerController* ResolveOwningPlayerController() const;
	void SetPopupModalInput(bool bEnabled, UUserWidget* FocusWidget = nullptr);
	void TryShowNext();
	void HandlePopupConfirmed();
	void HandlePopupCancelled();
	void HandleFormPopupSubmitted(const FMCPFormPopupResult& Result);
	void HandleFormPopupCancelled(const FMCPFormPopupResult& Result);
	void HandleMessagePopupClosed();
	void HandleActiveConfirmPopupResult(bool bConfirmed);
	void HandleActiveFormPopupResult(const FMCPFormPopupResult& Result);
	void HandleActiveMessagePopupClosed();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPConfirmPopupWidget> ConfirmPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPFormPopupWidget> FormPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPBlurMessagePopupWidget> MessagePopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPConfirmPopupWidget> ConfirmPopupInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMCPFormPopupWidget> FormPopupInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMCPBlurMessagePopupWidget> MessagePopupInstance;

	TArray<FMCPQueuedPopupRequest> PendingRequests;
	TOptional<FMCPQueuedPopupRequest> ActiveRequest;
	bool bPopupActive = false;
};
