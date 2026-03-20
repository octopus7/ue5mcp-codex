// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPItemTileDataObject.h"

void UMCPItemTileDataObject::SetTileData(const FName InItemId, UTexture2D* InItemTexture, const int32 InQuantity)
{
	ItemId = InItemId;
	ItemTexture = InItemTexture;
	Quantity = FMath::Max(InQuantity, 0);
}
