#pragma once

#include "CoreMinimal.h"

struct FMCPWidgetPatchOperation
{
	FString Op;
	FString Key;
	FString ParentKey;
	FString NewParentKey;
	FString WidgetClass;
	FString WidgetName;
	FString VariableName;
	FString EventName;
	FString FunctionName;
	TMap<FString, FString> StringProps;
	TMap<FString, bool> BoolProps;
	TMap<FString, double> NumberProps;
};

struct FMCPWidgetPatchRequest
{
	FString SchemaVersion;
	FString Mode;
	FString TargetWidgetPath;
	FString ParentClassPath;
	TArray<FMCPWidgetPatchOperation> Operations;
};
