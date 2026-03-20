// Copyright Epic Games, Inc. All Rights Reserved.

#include "OctoMCPModule.h"

	TSharedRef<FJsonObject> FOctoMCPModule::BuildSetGlobalDefaultGameModeObject(
		const FString& GameModeClassPath,
		const bool bSaveConfig) const
	{
		const FSetGlobalDefaultGameModeResult SetResult =
			SetGlobalDefaultGameMode(GameModeClassPath, bSaveConfig);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("saved"), SetResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), SetResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), SetResult.Message);
		ResultObject->SetStringField(TEXT("gameModeClassPath"), SetResult.GameModeClassPath);
		return ResultObject;
	}

	FSetGlobalDefaultGameModeResult FOctoMCPModule::SetGlobalDefaultGameMode(
		const FString& InGameModeClassPath,
		const bool bSaveConfig) const
	{
		FSetGlobalDefaultGameModeResult Result;

		FString ErrorMessage;
		UClass* const GameModeClass =
			ResolveClassReference(InGameModeClassPath, AGameModeBase::StaticClass(), Result.GameModeClassPath, ErrorMessage);
		if (GameModeClass == nullptr)
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		UGameMapsSettings::SetGlobalDefaultGameMode(Result.GameModeClassPath);

		if (bSaveConfig)
		{
			UGameMapsSettings* const GameMapsSettings = GetMutableDefault<UGameMapsSettings>();
			if (GameMapsSettings == nullptr)
			{
				Result.Message = TEXT("Could not access GameMapsSettings to save the default game mode.");
				return Result;
			}

			GameMapsSettings->SaveConfig();
			Result.bSaved = GameMapsSettings->TryUpdateDefaultConfigFile(TEXT(""), false);
			if (!Result.bSaved)
			{
				Result.Message = TEXT("Updated the default game mode in memory but failed to write DefaultEngine.ini.");
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Set the global default game mode to %s."),
			*Result.GameModeClassPath);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildBootstrapProjectMapObject(
		const FString& LevelFileName,
		const FString& DirectoryPath,
		const bool bForceCreate) const
	{
		const FBootstrapProjectMapResult BootstrapResult =
			BootstrapProjectMap(LevelFileName, DirectoryPath, bForceCreate);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("bootstrapped"), BootstrapResult.bBootstrapped);
		ResultObject->SetBoolField(TEXT("created"), BootstrapResult.bCreated);
		ResultObject->SetBoolField(TEXT("forceCreate"), BootstrapResult.bForceCreate);
		ResultObject->SetBoolField(TEXT("savedConfig"), BootstrapResult.bSavedConfig);
		ResultObject->SetBoolField(TEXT("success"), BootstrapResult.bSuccess);
		ResultObject->SetNumberField(TEXT("existingLevelCount"), BootstrapResult.ExistingLevelCount);
		ResultObject->SetStringField(TEXT("message"), BootstrapResult.Message);
		ResultObject->SetStringField(TEXT("levelFileName"), BootstrapResult.LevelFileName);
		ResultObject->SetStringField(TEXT("directoryPath"), BootstrapResult.DirectoryPath);
		ResultObject->SetStringField(TEXT("levelAssetPath"), BootstrapResult.LevelAssetPath);
		ResultObject->SetStringField(TEXT("levelObjectPath"), BootstrapResult.LevelObjectPath);
		return ResultObject;
	}

	FBootstrapProjectMapResult FOctoMCPModule::BootstrapProjectMap(
		const FString& InLevelFileName,
		const FString& InDirectoryPath,
		const bool bForceCreate) const
	{
		FBootstrapProjectMapResult Result;
		Result.bForceCreate = bForceCreate;

		Result.LevelFileName = InLevelFileName.TrimStartAndEnd();
		if (Result.LevelFileName.IsEmpty())
		{
			Result.Message = TEXT("levelFileName must not be empty.");
			return Result;
		}

		if (Result.LevelFileName.Contains(TEXT("/"))
			|| Result.LevelFileName.Contains(TEXT("\\"))
			|| Result.LevelFileName.Contains(TEXT(".")))
		{
			Result.Message = TEXT("levelFileName must be a plain asset name without path separators or dots.");
			return Result;
		}

		Result.DirectoryPath = InDirectoryPath.TrimStartAndEnd();
		while (Result.DirectoryPath.EndsWith(TEXT("/")))
		{
			Result.DirectoryPath.LeftChopInline(1, EAllowShrinking::No);
		}

		if (Result.DirectoryPath.IsEmpty())
		{
			Result.Message = TEXT("directoryPath must not be empty.");
			return Result;
		}

		if (Result.DirectoryPath != TEXT("/Game") && !Result.DirectoryPath.StartsWith(TEXT("/Game/")))
		{
			Result.Message = TEXT("directoryPath must be under /Game.");
			return Result;
		}

		Result.LevelAssetPath = FString::Printf(TEXT("%s/%s"), *Result.DirectoryPath, *Result.LevelFileName);
		Result.LevelObjectPath = FString::Printf(TEXT("%s.%s"), *Result.LevelAssetPath, *Result.LevelFileName);

		FText ValidationError;
		if (!FPackageName::IsValidLongPackageName(Result.LevelAssetPath, false, &ValidationError))
		{
			Result.Message = ValidationError.ToString();
			return Result;
		}

		if (FPackageName::DoesPackageExist(Result.LevelAssetPath))
		{
			Result.Message = FString::Printf(
				TEXT("A package already exists at %s."),
				*Result.LevelAssetPath);
			return Result;
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		TArray<FString> PathsToScan;
		PathsToScan.Add(TEXT("/Game"));
		AssetRegistry.ScanPathsSynchronous(PathsToScan, false);

		TArray<FAssetData> WorldAssets;
		AssetRegistry.GetAssetsByClass(UWorld::StaticClass()->GetClassPathName(), WorldAssets, true);
		for (const FAssetData& AssetData : WorldAssets)
		{
			const FString PackageName = AssetData.PackageName.ToString();
			if (PackageName == TEXT("/Game") || PackageName.StartsWith(TEXT("/Game/")))
			{
				++Result.ExistingLevelCount;
			}
		}

		if (!Result.bForceCreate && Result.ExistingLevelCount > 0)
		{
			Result.bSuccess = true;
			Result.Message = FString::Printf(
				TEXT("Project already contains %d level(s) under /Game. Skipped bootstrap."),
				Result.ExistingLevelCount);
			return Result;
		}

		UWorld* const NewWorld = UEditorLoadingAndSavingUtils::NewMapFromTemplate(OctoMCP::BootstrapTemplateMapPath, true);
		if (NewWorld == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not create a new map from template %s."),
				OctoMCP::BootstrapTemplateMapPath);
			return Result;
		}

		if (!UEditorLoadingAndSavingUtils::SaveMap(NewWorld, Result.LevelAssetPath))
		{
			Result.Message = FString::Printf(
				TEXT("Created a map from template %s but failed to save it as %s."),
				OctoMCP::BootstrapTemplateMapPath,
				*Result.LevelAssetPath);
			return Result;
		}

		Result.bCreated = true;

		UGameMapsSettings* const GameMapsSettings = GetMutableDefault<UGameMapsSettings>();
		if (GameMapsSettings == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Created the bootstrap map %s but could not access GameMapsSettings."),
				*Result.LevelObjectPath);
			return Result;
		}

		UGameMapsSettings::SetGameDefaultMap(Result.LevelObjectPath);
#if WITH_EDITORONLY_DATA
		GameMapsSettings->EditorStartupMap = FSoftObjectPath(Result.LevelObjectPath);
#endif

		GameMapsSettings->SaveConfig();
		Result.bSavedConfig = GameMapsSettings->TryUpdateDefaultConfigFile(TEXT(""), false);
		if (!Result.bSavedConfig)
		{
			Result.Message = FString::Printf(
				TEXT("Created the bootstrap map %s but failed to write DefaultEngine.ini."),
				*Result.LevelObjectPath);
			return Result;
		}

		Result.bBootstrapped = true;
		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Bootstrapped %s from %s and set it as the GameDefaultMap and EditorStartupMap (forceCreate=%s)."),
			*Result.LevelObjectPath,
			OctoMCP::BootstrapTemplateMapPath,
			Result.bForceCreate ? TEXT("true") : TEXT("false"));
		return Result;
	}
