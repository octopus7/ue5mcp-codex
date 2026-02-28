#pragma once

#include "CoreMinimal.h"
#include "MCPConfirmPopupWidget.h"
#include "MCPFormPopupWidget.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MCPPopupManagerSubsystem.generated.h"

class APlayerController;
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
	bool IsConfirmPopupActive() const;
	bool IsAnyPopupActive() const;
	int32 GetPendingConfirmPopupCount() const;
	int32 GetPendingPopupCount() const;

private:
	enum class EMCPPopupRequestType : uint8
	{
		Confirm,
		Form
	};

	static constexpr int32 MaxPendingRequests = 32;

	void ResolveConfirmPopupClass();
	UMCPConfirmPopupWidget* GetOrCreateConfirmPopup();
	void ResolveFormPopupClass();
	UMCPFormPopupWidget* GetOrCreateFormPopup();
	APlayerController* ResolveOwningPlayerController() const;
	void SetPopupModalInput(bool bEnabled, UUserWidget* FocusWidget = nullptr);
	void TryShowNext();
	void HandlePopupConfirmed();
	void HandlePopupCancelled();
	void HandleActiveConfirmPopupResult(bool bConfirmed);
	void HandleFormPopupSubmitted(const FMCPFormPopupResult& Result);
	void HandleFormPopupCancelled(const FMCPFormPopupResult& Result);
	void HandleActiveFormPopupResult(const FMCPFormPopupResult& Result);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPConfirmPopupWidget> ConfirmPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Popup")
	TSubclassOf<UMCPFormPopupWidget> FormPopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPConfirmPopupWidget> ActiveConfirmPopupInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMCPFormPopupWidget> ActiveFormPopupInstance;

	TArray<EMCPPopupRequestType> PendingPopupOrder;
	TArray<FMCPConfirmPopupRequest> PendingConfirmRequests;
	TArray<FMCPFormPopupRequest> PendingFormRequests;
	TOptional<EMCPPopupRequestType> ActivePopupType;
	TOptional<FMCPConfirmPopupRequest> ActiveConfirmRequest;
	TOptional<FMCPFormPopupRequest> ActiveFormRequest;
	bool bPopupActive = false;
};
