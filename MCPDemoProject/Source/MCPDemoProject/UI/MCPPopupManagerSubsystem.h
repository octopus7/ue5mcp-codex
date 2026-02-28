#pragma once

#include "CoreMinimal.h"
#include "MCPConfirmPopupWidget.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MCPPopupManagerSubsystem.generated.h"

class APlayerController;
class UMCPConfirmPopupWidget;
class UUserWidget;

UCLASS()
class MCPDEMOPROJECT_API UMCPPopupManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	bool RequestConfirmPopup(FMCPConfirmPopupRequest Request);
	bool IsConfirmPopupActive() const;
	int32 GetPendingConfirmPopupCount() const;

private:
	static constexpr int32 MaxPendingRequests = 32;

	void ResolveConfirmPopupClass();
	UMCPConfirmPopupWidget* GetOrCreateConfirmPopup();
	APlayerController* ResolveOwningPlayerController() const;
	void SetPopupModalInput(bool bEnabled, UUserWidget* FocusWidget = nullptr);
	void TryShowNext();
	void HandlePopupConfirmed();
	void HandlePopupCancelled();
	void HandleActivePopupResult(bool bConfirmed);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPConfirmPopupWidget> ConfirmPopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPConfirmPopupWidget> ActivePopupInstance;

	TArray<FMCPConfirmPopupRequest> PendingRequests;
	TOptional<FMCPConfirmPopupRequest> ActiveRequest;
	bool bPopupActive = false;
};

