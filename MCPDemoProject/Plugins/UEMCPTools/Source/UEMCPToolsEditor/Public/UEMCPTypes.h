#pragma once

#include "CoreMinimal.h"
#include "UEMCPTypes.generated.h"

UENUM(BlueprintType)
enum class EMCPInvokeStatus : uint8
{
	Success UMETA(DisplayName = "Success"),
	ValidationError UMETA(DisplayName = "ValidationError"),
	ExecutionError UMETA(DisplayName = "ExecutionError"),
	Unauthorized UMETA(DisplayName = "Unauthorized"),
	UnknownTool UMETA(DisplayName = "UnknownTool")
};

USTRUCT(BlueprintType)
struct FMCPExecutionMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	FString Code;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	FString Detail;
};

USTRUCT(BlueprintType)
struct FMCPInvokeRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	FString RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	FString ToolId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	FString PayloadJson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	bool bDryRun = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	FString AuthToken;
};

USTRUCT(BlueprintType)
struct FMCPInvokeResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	FString RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	EMCPInvokeStatus Status = EMCPInvokeStatus::Success;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	int32 AppliedCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	int32 SkippedCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	FString DiffJson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	TArray<FMCPExecutionMessage> Warnings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	TArray<FMCPExecutionMessage> Errors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MCP")
	bool bHasFatalError = false;

	void AddWarning(const FString& InCode, const FString& InMessage, const FString& InDetail = FString())
	{
		FMCPExecutionMessage& Warning = Warnings.AddDefaulted_GetRef();
		Warning.Code = InCode;
		Warning.Message = InMessage;
		Warning.Detail = InDetail;
	}

	void AddError(const FString& InCode, const FString& InMessage, const FString& InDetail = FString(), bool bFatal = false)
	{
		FMCPExecutionMessage& Error = Errors.AddDefaulted_GetRef();
		Error.Code = InCode;
		Error.Message = InMessage;
		Error.Detail = InDetail;
		bHasFatalError |= bFatal;
	}

	bool HasErrors() const
	{
		return Errors.Num() > 0;
	}
};
