#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "UEMCPTypes.h"
#include "UEMCPEditorSubsystem.generated.h"

UCLASS()
class UEMCPTOOLSEDITOR_API UUEMCPEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "UE MCP")
	bool InvokeTool(const FMCPInvokeRequest& Request, FMCPInvokeResponse& OutResponse);

	bool ParseAndInvokeJson(const FString& RequestBodyJson, FMCPInvokeResponse& OutResponse, const FString& AuthToken = FString());
};
