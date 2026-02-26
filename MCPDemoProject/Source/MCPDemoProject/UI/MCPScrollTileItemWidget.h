#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "MCPScrollTileItemWidget.generated.h"

class UBorder;
class UTextBlock;
class UMCPScrollTileItemObject;

UCLASS()
class MCPDEMOPROJECT_API UMCPScrollTileItemWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

private:
	void BuildFallbackWidgetTreeIfNeeded();
	void ApplyItemObject(const UMCPScrollTileItemObject* ItemObject);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> MCP_ItemBackground;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MCP_ItemNameText;
};
