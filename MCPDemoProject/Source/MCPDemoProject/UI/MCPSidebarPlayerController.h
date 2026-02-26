#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MCPSidebarPlayerController.generated.h"

class UMCPSidebarWidget;

UCLASS()
class MCPDEMOPROJECT_API AMCPSidebarPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMCPSidebarPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void BeginPlayingState() override;

private:
	void ResolveSidebarWidgetClass();
	void TryCreateSidebarWidget();

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMCPSidebarWidget> SidebarWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMCPSidebarWidget> SidebarWidgetInstance;
};
