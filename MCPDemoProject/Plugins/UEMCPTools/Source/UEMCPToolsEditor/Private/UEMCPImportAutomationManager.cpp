#include "UEMCPImportAutomationManager.h"

#include "Editor.h"
#include "Factories/Factory.h"
#include "Subsystems/ImportSubsystem.h"
#include "Engine/Texture2D.h"
#include "UEMCPImportSettings.h"
#include "UEMCPImportTools.h"
#include "UEMCPLog.h"
#include "UEMCPTypes.h"

void FUEMCPImportAutomationManager::Start()
{
	if (PostImportHandle.IsValid() || GEditor == nullptr)
	{
		return;
	}

	if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
	{
		PostImportHandle = ImportSubsystem->OnAssetPostImport.AddRaw(this, &FUEMCPImportAutomationManager::HandleAssetPostImport);
		UE_LOG(LogUEMCPTools, Log, TEXT("Import automation manager started."));
	}
}

void FUEMCPImportAutomationManager::Stop()
{
	if (!PostImportHandle.IsValid() || GEditor == nullptr)
	{
		return;
	}

	if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
	{
		ImportSubsystem->OnAssetPostImport.Remove(PostImportHandle);
	}

	PostImportHandle.Reset();
	UE_LOG(LogUEMCPTools, Log, TEXT("Import automation manager stopped."));
}

void FUEMCPImportAutomationManager::HandleAssetPostImport(UFactory* InFactory, UObject* InObject)
{
	if (InFactory == nullptr || InObject == nullptr)
	{
		return;
	}

	UTexture2D* Texture = Cast<UTexture2D>(InObject);
	if (Texture == nullptr)
	{
		return;
	}

	const UUEMCPImportSettings* Settings = GetDefault<UUEMCPImportSettings>();
	if (Settings == nullptr)
	{
		return;
	}

	FMCPInvokeResponse TempResponse;
	FUEMCPImportTools::ApplyRulesToTexture(Texture, Settings->Rules, false, &TempResponse);
	if (TempResponse.AppliedCount > 0)
	{
		UE_LOG(LogUEMCPTools, Log, TEXT("Applied UI texture import preset: %s"), *Texture->GetPathName());
	}
}
