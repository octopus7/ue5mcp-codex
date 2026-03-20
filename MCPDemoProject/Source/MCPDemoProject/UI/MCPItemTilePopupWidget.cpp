// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPItemTilePopupWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Engine/Texture2D.h"
#include "MCPItemTileDataObject.h"
#include "Misc/PackageName.h"

namespace
{
	struct FMCPHardcodedTileItemSpec
	{
		const TCHAR* TextureObjectPath;
		int32 DefaultQuantity;
	};

	static const FMCPHardcodedTileItemSpec GMCPHardcodedTileItemSpecs[] = {
		{TEXT("/Game/UI/Image/T_BentoBox.T_BentoBox"), 3},
		{TEXT("/Game/UI/Image/T_CuteChefHat.T_CuteChefHat"), 1},
		{TEXT("/Game/UI/Image/T_FruitTart.T_FruitTart"), 5},
		{TEXT("/Game/UI/Image/T_HeartShapedCookie.T_HeartShapedCookie"), 7},
		{TEXT("/Game/UI/Image/T_MagicSpoon.T_MagicSpoon"), 2},
		{TEXT("/Game/UI/Image/T_MilkCartonWithCatFace.T_MilkCartonWithCatFace"), 4},
		{TEXT("/Game/UI/Image/T_MiniOven.T_MiniOven"), 1},
		{TEXT("/Game/UI/Image/T_PancakeStack.T_PancakeStack"), 6},
		{TEXT("/Game/UI/Image/T_PastelMacaron.T_PastelMacaron"), 8},
		{TEXT("/Game/UI/Image/T_PinkWhisk.T_PinkWhisk"), 2},
		{TEXT("/Game/UI/Image/T_Pudding.T_Pudding"), 9},
		{TEXT("/Game/UI/Image/T_RainbowDonut.T_RainbowDonut"), 4},
		{TEXT("/Game/UI/Image/T_RecipeBookWithRibbon.T_RecipeBookWithRibbon"), 1},
		{TEXT("/Game/UI/Image/T_StarCandyJar.T_StarCandyJar"), 3},
		{TEXT("/Game/UI/Image/T_StrawberryCake.T_StrawberryCake"), 5},
		{TEXT("/Game/UI/Image/T_TeaCupWithBearFace.T_TeaCupWithBearFace"), 2},
	};
}

void UMCPItemTilePopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ClearButton != nullptr)
	{
		ClearButton->OnClicked.RemoveAll(this);
		ClearButton->OnClicked.AddDynamic(this, &UMCPItemTilePopupWidget::HandleClearButtonClicked);
	}

	if (RandomizeButton != nullptr)
	{
		RandomizeButton->OnClicked.RemoveAll(this);
		RandomizeButton->OnClicked.AddDynamic(this, &UMCPItemTilePopupWidget::HandleRandomizeButtonClicked);
	}
}

void UMCPItemTilePopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (TitleText != nullptr)
	{
		TitleText->SetText(FText::FromString(TEXT("Tile Item List Test")));
	}

	PopulateDefaultItems();
}

void UMCPItemTilePopupWidget::HandleClearButtonClicked()
{
	ClearItems();
}

void UMCPItemTilePopupWidget::HandleRandomizeButtonClicked()
{
	PopulateRandomizedItems();
}

void UMCPItemTilePopupWidget::PopulateDefaultItems()
{
	TArray<int32> ItemIndices;
	ItemIndices.Reserve(UE_ARRAY_COUNT(GMCPHardcodedTileItemSpecs));
	for (int32 ItemIndex = 0; ItemIndex < UE_ARRAY_COUNT(GMCPHardcodedTileItemSpecs); ++ItemIndex)
	{
		ItemIndices.Add(ItemIndex);
	}

	RebuildItemsFromIndices(ItemIndices, false);
}

void UMCPItemTilePopupWidget::PopulateRandomizedItems()
{
	TArray<int32> ItemIndices;
	ItemIndices.Reserve(UE_ARRAY_COUNT(GMCPHardcodedTileItemSpecs));
	for (int32 ItemIndex = 0; ItemIndex < UE_ARRAY_COUNT(GMCPHardcodedTileItemSpecs); ++ItemIndex)
	{
		ItemIndices.Add(ItemIndex);
	}

	for (int32 ItemIndex = ItemIndices.Num() - 1; ItemIndex > 0; --ItemIndex)
	{
		const int32 SwapIndex = FMath::RandRange(0, ItemIndex);
		ItemIndices.Swap(ItemIndex, SwapIndex);
	}

	const int32 RandomCount = FMath::RandRange(12, ItemIndices.Num());
	ItemIndices.SetNum(RandomCount);

	RebuildItemsFromIndices(ItemIndices, true);
}

void UMCPItemTilePopupWidget::ClearItems()
{
	TileItems.Empty();

	if (ItemTileView != nullptr)
	{
		ItemTileView->ClearListItems();
	}
}

void UMCPItemTilePopupWidget::RebuildItemsFromIndices(const TArray<int32>& ItemIndices, const bool bUseRandomQuantities)
{
	ClearItems();

	for (const int32 ItemIndex : ItemIndices)
	{
		if (!GMCPHardcodedTileItemSpecs[ItemIndex].TextureObjectPath)
		{
			continue;
		}

		const int32 Quantity = bUseRandomQuantities
			? FMath::RandRange(1, 99)
			: GMCPHardcodedTileItemSpecs[ItemIndex].DefaultQuantity;
		AddItemFromTexturePath(GMCPHardcodedTileItemSpecs[ItemIndex].TextureObjectPath, Quantity);
	}
}

void UMCPItemTilePopupWidget::AddItemFromTexturePath(const TCHAR* TextureObjectPath, const int32 Quantity)
{
	if (ItemTileView == nullptr || TextureObjectPath == nullptr || TextureObjectPath[0] == 0)
	{
		return;
	}

	UTexture2D* const ItemTexture = LoadObject<UTexture2D>(nullptr, TextureObjectPath);
	if (ItemTexture == nullptr)
	{
		return;
	}

	UMCPItemTileDataObject* const TileItem = NewObject<UMCPItemTileDataObject>(this);
	if (TileItem == nullptr)
	{
		return;
	}

	const FString AssetName = FPackageName::ObjectPathToObjectName(FString(TextureObjectPath));
	TileItem->SetTileData(FName(*AssetName), ItemTexture, Quantity);

	TileItems.Add(TileItem);
	ItemTileView->AddItem(TileItem);
}
