#pragma once

#include "CoreMinimal.h"

class UFactory;
class UObject;

class FUEMCPImportAutomationManager
{
public:
	void Start();
	void Stop();

private:
	void HandleAssetPostImport(UFactory* InFactory, UObject* InObject);

private:
	FDelegateHandle PostImportHandle;
};
