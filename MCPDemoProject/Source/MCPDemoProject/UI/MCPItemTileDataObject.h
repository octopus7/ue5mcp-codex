// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MCPItemTileDataObject.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class MCPDEMOPROJECT_API UMCPItemTileDataObject : public UObject
{
	GENERATED_BODY()

public:
	void SetTileData(const FName InItemId, UTexture2D* InItemTexture, const int32 InQuantity);

	UFUNCTION(BlueprintPure, Category = "Item Tile")
	FName GetItemId() const
	{
		return ItemId;
	}

	UFUNCTION(BlueprintPure, Category = "Item Tile")
	UTexture2D* GetItemTexture() const
	{
		return ItemTexture;
	}

	UFUNCTION(BlueprintPure, Category = "Item Tile")
	int32 GetQuantity() const
	{
		return Quantity;
	}

private:
	UPROPERTY(Transient)
	FName ItemId = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ItemTexture = nullptr;

	UPROPERTY(Transient)
	int32 Quantity = 0;
};
