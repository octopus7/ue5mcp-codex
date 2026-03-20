// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "MCPItemTileEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UObject;

UCLASS(BlueprintType, Blueprintable)
class MCPDEMOPROJECT_API UMCPItemTileEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(BlueprintReadOnly, Category = "Item Tile", meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemImage = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Item Tile", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText = nullptr;
};
