#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MCPScrollTileItemObject.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class MCPDEMOPROJECT_API UMCPScrollTileItemObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollTile")
	FLinearColor BackgroundColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollTile")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScrollTile")
	TObjectPtr<UTexture2D> IconTexture = nullptr;
};
