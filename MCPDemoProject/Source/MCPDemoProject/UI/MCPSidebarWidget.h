#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPSidebarWidget.generated.h"

class UBorder;
class UButton;
class UVerticalBox;

UCLASS()
class MCPDEMOPROJECT_API UMCPSidebarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	UFUNCTION()
	void HandleMenu1Clicked();

	UFUNCTION()
	void HandleMenu2Clicked();

	UFUNCTION()
	void HandleMenu3Clicked();

private:
	void BuildSidebarLayout();
	UButton* CreateMenuButton(UVerticalBox* ParentBox, const FString& Label, const FLinearColor& ButtonTint);
	void ShowDebugMessage(const FString& Message, const FColor& Color) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UButton> Menu1Button;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Menu2Button;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Menu3Button;
};
