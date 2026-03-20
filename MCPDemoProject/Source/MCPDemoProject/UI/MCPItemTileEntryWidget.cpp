// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPItemTileEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "MCPItemTileDataObject.h"

void UMCPItemTileEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	const UMCPItemTileDataObject* const TileItem = Cast<UMCPItemTileDataObject>(ListItemObject);

	if (ItemImage != nullptr)
	{
		ItemImage->SetBrushFromTexture(TileItem != nullptr ? TileItem->GetItemTexture() : nullptr, true);
	}

	if (QuantityText != nullptr)
	{
		const int32 Quantity = TileItem != nullptr ? TileItem->GetQuantity() : 0;
		QuantityText->SetText(FText::FromString(FString::Printf(TEXT("x%d"), Quantity)));
	}

	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
}
