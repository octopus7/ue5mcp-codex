#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCPScrollGridItemWidget.generated.h"

class UBorder;
class USizeBox;
class UTextBlock;

USTRUCT(BlueprintType)
struct FMCPScrollGridItemData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollGrid")
	FLinearColor BackgroundColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollGrid")
	FText Name;
};

UCLASS()
class MCPDEMOPROJECT_API UMCPScrollGridItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void ApplyItemData(const FMCPScrollGridItemData& InData);

private:
	void BuildFallbackWidgetTreeIfNeeded();

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> MCP_ItemBackground;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_ItemNameText;
};

