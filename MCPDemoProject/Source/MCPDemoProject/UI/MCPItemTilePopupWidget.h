// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCPPopupWidget.h"
#include "MCPItemTilePopupWidget.generated.h"

class UButton;
class UMCPItemTileDataObject;
class UTileView;

UCLASS(BlueprintType, Blueprintable)
class MCPDEMOPROJECT_API UMCPItemTilePopupWidget : public UMCPPopupWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClearButtonClicked();

	UFUNCTION()
	void HandleRandomizeButtonClicked();

	void PopulateDefaultItems();
	void PopulateRandomizedItems();
	void ClearItems();
	void RebuildItemsFromIndices(const TArray<int32>& ItemIndices, bool bUseRandomQuantities);
	void AddItemFromTexturePath(const TCHAR* TextureObjectPath, const int32 Quantity);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Item Tile Popup", meta = (BindWidgetOptional))
	TObjectPtr<UTileView> ItemTileView = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Item Tile Popup", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ClearButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Item Tile Popup", meta = (BindWidgetOptional))
	TObjectPtr<UButton> RandomizeButton = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMCPItemTileDataObject>> TileItems;
};
