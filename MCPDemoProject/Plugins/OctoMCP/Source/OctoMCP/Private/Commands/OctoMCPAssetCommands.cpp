// Copyright Epic Games, Inc. All Rights Reserved.

#include "OctoMCPModule.h"


	TSharedRef<FJsonObject> FOctoMCPModule::BuildCreateBlueprintAssetObject(
		const FString& AssetPath,
		const FString& ParentClassPath,
		const bool bSaveAsset) const
	{
		const FCreateBlueprintAssetResult CreateResult =
			CreateBlueprintAsset(AssetPath, ParentClassPath, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("created"), CreateResult.bCreated);
		ResultObject->SetBoolField(TEXT("saved"), CreateResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), CreateResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), CreateResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), CreateResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), CreateResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), CreateResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), CreateResult.AssetName);
		ResultObject->SetStringField(TEXT("parentClassPath"), CreateResult.ParentClassPath);
		ResultObject->SetStringField(TEXT("parentClassName"), CreateResult.ParentClassName);
		ResultObject->SetStringField(TEXT("generatedClassPath"), CreateResult.GeneratedClassPath);
		return ResultObject;
	}

	FCreateBlueprintAssetResult FOctoMCPModule::CreateBlueprintAsset(
		const FString& InAssetPath,
		const FString& InParentClassPath,
		const bool bSaveAsset) const
	{
		FCreateBlueprintAssetResult Result;

		FString AssetPackageName;
		FString AssetObjectPath;
		FString ErrorMessage;
		if (!NormalizeWidgetBlueprintAssetPath(
				InAssetPath,
				AssetPackageName,
				Result.PackagePath,
				Result.AssetName,
				AssetObjectPath,
				ErrorMessage))
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		Result.AssetPath = AssetPackageName;
		Result.AssetObjectPath = AssetObjectPath;

		UClass* ParentClass = ResolveClassReference(InParentClassPath, nullptr, Result.ParentClassPath, ErrorMessage);
		if (ParentClass == nullptr)
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		Result.ParentClassName = ParentClass->GetName();

		if (ParentClass->IsChildOf(UUserWidget::StaticClass()))
		{
			Result.Message = FString::Printf(
				TEXT("Parent class %s derives from UUserWidget. Use create_widget_blueprint instead."),
				*Result.ParentClassPath);
			return Result;
		}

		if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
		{
			Result.Message = FString::Printf(
				TEXT("Cannot create a Blueprint from parent class: %s"),
				*Result.ParentClassPath);
			return Result;
		}

		if (FindObject<UObject>(nullptr, *AssetObjectPath) != nullptr || LoadObject<UObject>(nullptr, *AssetObjectPath) != nullptr)
		{
			Result.Message = FString::Printf(TEXT("Asset already exists: %s"), *AssetObjectPath);
			return Result;
		}

		UBlueprintFactory* const Factory = NewObject<UBlueprintFactory>();
		Factory->BlueprintType = BPTYPE_Normal;
		Factory->ParentClass = ParentClass;
		Factory->bEditAfterNew = false;
		Factory->bSkipClassPicker = true;

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		UObject* const NewAsset =
			AssetTools.CreateAsset(Result.AssetName, Result.PackagePath, UBlueprint::StaticClass(), Factory, TEXT("OctoMCP"));
		UBlueprint* const NewBlueprint = Cast<UBlueprint>(NewAsset);
		if (NewBlueprint == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Failed to create Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		Result.bCreated = true;
		Result.AssetObjectPath = NewBlueprint->GetPathName();
		Result.AssetPath = NewBlueprint->GetOutermost()->GetName();
		NewBlueprint->MarkPackageDirty();

		FKismetEditorUtilities::CompileBlueprint(NewBlueprint);
		if (NewBlueprint->GeneratedClass != nullptr)
		{
			Result.GeneratedClassPath = NewBlueprint->GeneratedClass->GetPathName();
		}

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Created Blueprint but could not access the EditorAssetSubsystem to save it: %s"),
					*Result.AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(NewBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Created Blueprint but failed to save it: %s"),
					*Result.AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Created Blueprint %s using parent class %s."),
			*Result.AssetObjectPath,
			*Result.ParentClassPath);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildCreateWidgetBlueprintObject(
		const FString& AssetPath,
		const FString& ParentClassPath,
		const bool bSaveAsset) const
	{
		const FCreateWidgetBlueprintResult CreateResult =
			CreateWidgetBlueprintAsset(AssetPath, ParentClassPath, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("created"), CreateResult.bCreated);
		ResultObject->SetBoolField(TEXT("saved"), CreateResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), CreateResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), CreateResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), CreateResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), CreateResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), CreateResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), CreateResult.AssetName);
		ResultObject->SetStringField(TEXT("parentClassPath"), CreateResult.ParentClassPath);
		ResultObject->SetStringField(TEXT("parentClassName"), CreateResult.ParentClassName);
		return ResultObject;
	}

	FCreateWidgetBlueprintResult FOctoMCPModule::CreateWidgetBlueprintAsset(
		const FString& InAssetPath,
		const FString& InParentClassPath,
		const bool bSaveAsset) const
	{
		FCreateWidgetBlueprintResult Result;

		FString AssetPackageName;
		FString AssetObjectPath;
		FString ErrorMessage;
		if (!NormalizeWidgetBlueprintAssetPath(
				InAssetPath,
				AssetPackageName,
				Result.PackagePath,
				Result.AssetName,
				AssetObjectPath,
				ErrorMessage))
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		Result.AssetPath = AssetPackageName;
		Result.AssetObjectPath = AssetObjectPath;

		UClass* ParentClass = ResolveWidgetParentClass(InParentClassPath, Result.ParentClassPath, ErrorMessage);
		if (ParentClass == nullptr)
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		Result.ParentClassName = ParentClass->GetName();

		if (!ParentClass->IsChildOf(UUserWidget::StaticClass()))
		{
			Result.Message = FString::Printf(
				TEXT("Parent class must derive from UUserWidget: %s"),
				*Result.ParentClassPath);
			return Result;
		}

		if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
		{
			Result.Message = FString::Printf(
				TEXT("Cannot create a Widget Blueprint from parent class: %s"),
				*Result.ParentClassPath);
			return Result;
		}

		if (FindObject<UObject>(nullptr, *AssetObjectPath) != nullptr || LoadObject<UObject>(nullptr, *AssetObjectPath) != nullptr)
		{
			Result.Message = FString::Printf(TEXT("Asset already exists: %s"), *AssetObjectPath);
			return Result;
		}

		UWidgetBlueprintFactory* const Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->BlueprintType = BPTYPE_Normal;
		Factory->ParentClass = ParentClass;
		Factory->bEditAfterNew = false;

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		UObject* const NewAsset =
			AssetTools.CreateAsset(Result.AssetName, Result.PackagePath, UWidgetBlueprint::StaticClass(), Factory, TEXT("OctoMCP"));
		if (NewAsset == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Failed to create Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		Result.bCreated = true;
		Result.AssetObjectPath = NewAsset->GetPathName();
		Result.AssetPath = NewAsset->GetOutermost()->GetName();
		NewAsset->MarkPackageDirty();

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Created Widget Blueprint but could not access the EditorAssetSubsystem to save it: %s"),
					*Result.AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(NewAsset, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Created Widget Blueprint but failed to save it: %s"),
					*Result.AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Created Widget Blueprint %s using parent class %s."),
			*Result.AssetObjectPath,
			*Result.ParentClassPath);
		return Result;
	}

	bool FOctoMCPModule::NormalizeWidgetBlueprintAssetPath(
		const FString& InAssetPath,
		FString& OutAssetPackageName,
		FString& OutPackagePath,
		FString& OutAssetName,
		FString& OutAssetObjectPath,
		FString& OutError) const
	{
		const FString TrimmedAssetPath = InAssetPath.TrimStartAndEnd();
		if (TrimmedAssetPath.IsEmpty())
		{
			OutError = TEXT("assetPath must not be empty.");
			return false;
		}

		FText ValidationError;
		if (TrimmedAssetPath.Contains(TEXT(".")))
		{
			if (!FPackageName::IsValidObjectPath(TrimmedAssetPath, &ValidationError))
			{
				OutError = ValidationError.ToString();
				return false;
			}

			OutAssetPackageName = FPackageName::ObjectPathToPackageName(TrimmedAssetPath);
			OutAssetName = FPackageName::ObjectPathToObjectName(TrimmedAssetPath);
		}
		else
		{
			if (!FPackageName::IsValidLongPackageName(TrimmedAssetPath, false, &ValidationError))
			{
				OutError = ValidationError.ToString();
				return false;
			}

			OutAssetPackageName = TrimmedAssetPath;
			OutAssetName = FPackageName::GetLongPackageAssetName(OutAssetPackageName);
		}

		OutPackagePath = FPackageName::GetLongPackagePath(OutAssetPackageName);
		if (OutPackagePath.IsEmpty() || OutAssetName.IsEmpty())
		{
			OutError = TEXT("assetPath must include both a package path and an asset name.");
			return false;
		}

		OutAssetObjectPath = FString::Printf(TEXT("%s.%s"), *OutAssetPackageName, *OutAssetName);
		return true;
	}

	UClass* FOctoMCPModule::ResolveWidgetParentClass(
		const FString& InParentClassPath,
		FString& OutResolvedClassPath,
		FString& OutError) const
	{
		const FString TrimmedClassPath = InParentClassPath.TrimStartAndEnd();
		if (TrimmedClassPath.IsEmpty())
		{
			OutError = TEXT("parentClassPath must not be empty.");
			return nullptr;
		}

		TArray<FString> CandidatePaths;
		CandidatePaths.Add(TrimmedClassPath);

		if (!TrimmedClassPath.StartsWith(TEXT("/")))
		{
			FString NativeClassName = TrimmedClassPath;
			if (NativeClassName.StartsWith(TEXT("U")) && NativeClassName.Len() > 1)
			{
				NativeClassName.RightChopInline(1, EAllowShrinking::No);
			}

			CandidatePaths.Add(FString::Printf(TEXT("/Script/%s.%s"), FApp::GetProjectName(), *NativeClassName));
		}

		for (const FString& CandidatePath : CandidatePaths)
		{
			if (UClass* const ExistingClass = FindObject<UClass>(nullptr, *CandidatePath))
			{
				OutResolvedClassPath = ExistingClass->GetPathName();
				return ExistingClass;
			}

			if (UClass* const LoadedClass = LoadClass<UUserWidget>(nullptr, *CandidatePath, nullptr, LOAD_None, nullptr))
			{
				OutResolvedClassPath = LoadedClass->GetPathName();
				return LoadedClass;
			}
		}

		OutError = FString::Printf(TEXT("Could not resolve parent widget class: %s"), *TrimmedClassPath);
		return nullptr;
	}

	UClass* FOctoMCPModule::ResolveClassReference(
		const FString& InClassPath,
		const UClass* RequiredBaseClass,
		FString& OutResolvedClassPath,
		FString& OutError) const
	{
		const FString TrimmedClassPath = InClassPath.TrimStartAndEnd();
		if (TrimmedClassPath.IsEmpty())
		{
			OutError = TEXT("classPath must not be empty.");
			return nullptr;
		}

		TArray<FString> CandidateClassPaths;
		CandidateClassPaths.AddUnique(TrimmedClassPath);

		if (!TrimmedClassPath.StartsWith(TEXT("/")))
		{
			FString NativeClassName = TrimmedClassPath;
			if ((NativeClassName.StartsWith(TEXT("A")) || NativeClassName.StartsWith(TEXT("U"))) && NativeClassName.Len() > 1)
			{
				NativeClassName.RightChopInline(1, EAllowShrinking::No);
			}

			CandidateClassPaths.AddUnique(FString::Printf(TEXT("/Script/%s.%s"), FApp::GetProjectName(), *NativeClassName));
		}

		FString BlueprintAssetPath;
		FString IgnoredPackagePath;
		FString IgnoredAssetName;
		FString BlueprintAssetObjectPath;
		FString NormalizedAssetError;
		if (NormalizeWidgetBlueprintAssetPath(
				TrimmedClassPath,
				BlueprintAssetPath,
				IgnoredPackagePath,
				IgnoredAssetName,
				BlueprintAssetObjectPath,
				NormalizedAssetError))
		{
			CandidateClassPaths.AddUnique(FString::Printf(TEXT("%s.%s_C"), *BlueprintAssetPath, *IgnoredAssetName));
		}

		for (const FString& CandidateClassPath : CandidateClassPaths)
		{
			UClass* ResolvedClass = FindObject<UClass>(nullptr, *CandidateClassPath);
			if (ResolvedClass == nullptr)
			{
				ResolvedClass = LoadClass<UObject>(nullptr, *CandidateClassPath, nullptr, LOAD_None, nullptr);
			}

			if (ResolvedClass == nullptr)
			{
				continue;
			}

			if (RequiredBaseClass != nullptr && !ResolvedClass->IsChildOf(RequiredBaseClass))
			{
				OutError = FString::Printf(
					TEXT("Resolved class %s does not derive from required base class %s."),
					*ResolvedClass->GetPathName(),
					*RequiredBaseClass->GetPathName());
				return nullptr;
			}

			OutResolvedClassPath = ResolvedClass->GetPathName();
			return ResolvedClass;
		}

		if (!BlueprintAssetObjectPath.IsEmpty())
		{
			UBlueprint* BlueprintAsset = LoadObject<UBlueprint>(nullptr, *BlueprintAssetObjectPath);
			if (BlueprintAsset != nullptr)
			{
				if (BlueprintAsset->GeneratedClass == nullptr)
				{
					FKismetEditorUtilities::CompileBlueprint(BlueprintAsset);
				}

				if (UClass* const GeneratedClass = BlueprintAsset->GeneratedClass)
				{
					if (RequiredBaseClass != nullptr && !GeneratedClass->IsChildOf(RequiredBaseClass))
					{
						OutError = FString::Printf(
							TEXT("Resolved class %s does not derive from required base class %s."),
							*GeneratedClass->GetPathName(),
							*RequiredBaseClass->GetPathName());
						return nullptr;
					}

					OutResolvedClassPath = GeneratedClass->GetPathName();
					return GeneratedClass;
				}
			}
		}

		OutError = FString::Printf(TEXT("Could not resolve class reference: %s"), *TrimmedClassPath);
		return nullptr;
	}

	UTexture2D* FOctoMCPModule::ResolveTextureAsset(
		const FString& InTextureAssetPath,
		FString& OutResolvedAssetPath,
		FString& OutError) const
	{
		FString AssetPackageName;
		FString PackagePath;
		FString AssetName;
		FString AssetObjectPath;
		if (!NormalizeWidgetBlueprintAssetPath(
				InTextureAssetPath,
				AssetPackageName,
				PackagePath,
				AssetName,
				AssetObjectPath,
				OutError))
		{
			return nullptr;
		}

		UTexture2D* const TextureAsset = LoadObject<UTexture2D>(nullptr, *AssetObjectPath);
		if (TextureAsset == nullptr)
		{
			OutError = FString::Printf(TEXT("Could not load texture asset: %s"), *AssetObjectPath);
			return nullptr;
		}

		OutResolvedAssetPath = AssetObjectPath;
		return TextureAsset;
	}

	UClass* FOctoMCPModule::ResolveWidgetBlueprintGeneratedClass(
		const FString& InWidgetAssetPath,
		FString& OutResolvedAssetPath,
		FString& OutResolvedClassPath,
		FString& OutError) const
	{
		FString AssetPackageName;
		FString PackagePath;
		FString AssetName;
		FString AssetObjectPath;
		if (!NormalizeWidgetBlueprintAssetPath(
				InWidgetAssetPath,
				AssetPackageName,
				PackagePath,
				AssetName,
				AssetObjectPath,
				OutError))
		{
			return nullptr;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr)
		{
			OutError = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
		if (WidgetBlueprint->GeneratedClass == nullptr)
		{
			OutError = FString::Printf(
				TEXT("Widget Blueprint asset does not have a generated class after compile: %s"),
				*AssetObjectPath);
			return nullptr;
		}

		if (!WidgetBlueprint->GeneratedClass->IsChildOf(UUserWidget::StaticClass()))
		{
			OutError = FString::Printf(
				TEXT("Widget Blueprint generated class %s does not derive from UUserWidget."),
				*WidgetBlueprint->GeneratedClass->GetPathName());
			return nullptr;
		}

		OutResolvedAssetPath = AssetPackageName;
		OutResolvedClassPath = WidgetBlueprint->GeneratedClass->GetPathName();
		return WidgetBlueprint->GeneratedClass;
	}

	UWidget* FOctoMCPModule::EnsureWidgetInstanceOfClass(
		UWidgetBlueprint* WidgetBlueprint,
		UClass* WidgetClass,
		const TCHAR* WidgetName,
		bool& bOutCreated,
		bool& bOutReplaced,
		FString& OutError) const
	{
		bOutCreated = false;
		bOutReplaced = false;

		check(WidgetBlueprint != nullptr);
		check(WidgetBlueprint->WidgetTree != nullptr);
		check(WidgetClass != nullptr);

		const FName TargetName(WidgetName);
		UWidget* ExistingWidget = WidgetBlueprint->WidgetTree->FindWidget(TargetName);
		if (ExistingWidget != nullptr)
		{
			if (ExistingWidget->IsA(WidgetClass))
			{
				return ExistingWidget;
			}

			if (ExistingWidget->bIsVariable && FBlueprintEditorUtils::IsVariableUsed(WidgetBlueprint, ExistingWidget->GetFName()))
			{
				OutError = FString::Printf(
					TEXT("Widget %s is referenced in the graph and cannot be replaced automatically."),
					WidgetName);
				return nullptr;
			}

			TSet<UWidget*> WidgetsToDelete;
			WidgetsToDelete.Add(ExistingWidget);
			FWidgetBlueprintEditorUtils::DeleteWidgets(
				WidgetBlueprint,
				WidgetsToDelete,
				FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);

			if (WidgetBlueprint->WidgetTree->FindWidget(TargetName) != nullptr)
			{
				OutError = FString::Printf(
					TEXT("Failed to replace widget %s with a new instance of class %s."),
					WidgetName,
					*WidgetClass->GetPathName());
				return nullptr;
			}

			bOutReplaced = true;
		}

		bOutCreated = true;
		return WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, TargetName);
	}

	bool FOctoMCPModule::SetBoolPropertyValue(
		UClass* const OwnerClass,
		UObject* const ObjectInstance,
		const TCHAR* PropertyName,
		const bool bValue,
		FString& OutError) const
	{
		check(OwnerClass != nullptr);
		check(ObjectInstance != nullptr);

		FBoolProperty* const Property = CastField<FBoolProperty>(OwnerClass->FindPropertyByName(*FString(PropertyName)));
		if (Property == nullptr)
		{
			OutError = FString::Printf(
				TEXT("Boolean property %s was not found on %s."),
				PropertyName,
				*OwnerClass->GetPathName());
			return false;
		}

		Property->SetPropertyValue_InContainer(ObjectInstance, bValue);
		return true;
	}

	bool FOctoMCPModule::SetFloatPropertyValue(
		UClass* const OwnerClass,
		UObject* const ObjectInstance,
		const TCHAR* PropertyName,
		const float Value,
		FString& OutError) const
	{
		check(OwnerClass != nullptr);
		check(ObjectInstance != nullptr);

		FFloatProperty* const Property = CastField<FFloatProperty>(OwnerClass->FindPropertyByName(*FString(PropertyName)));
		if (Property == nullptr)
		{
			OutError = FString::Printf(
				TEXT("Float property %s was not found on %s."),
				PropertyName,
				*OwnerClass->GetPathName());
			return false;
		}

		Property->SetPropertyValue_InContainer(ObjectInstance, Value);
		return true;
	}

	bool FOctoMCPModule::SetClassPropertyValue(
		UClass* const OwnerClass,
		UObject* const ObjectInstance,
		const TCHAR* PropertyName,
		UClass* const Value,
		FString& OutError) const
	{
		check(OwnerClass != nullptr);
		check(ObjectInstance != nullptr);

		FClassProperty* const Property = CastField<FClassProperty>(OwnerClass->FindPropertyByName(*FString(PropertyName)));
		if (Property == nullptr)
		{
			OutError = FString::Printf(
				TEXT("Class property %s was not found on %s."),
				PropertyName,
				*OwnerClass->GetPathName());
			return false;
		}

		if (Value != nullptr && Property->MetaClass != nullptr && !Value->IsChildOf(Property->MetaClass))
		{
			OutError = FString::Printf(
				TEXT("Class %s is not compatible with property %s on %s."),
				*Value->GetPathName(),
				PropertyName,
				*OwnerClass->GetPathName());
			return false;
		}

		Property->SetObjectPropertyValue_InContainer(ObjectInstance, Value);
		return true;
	}

	bool FOctoMCPModule::SetNamePropertyValue(
		UClass* const OwnerClass,
		UObject* const ObjectInstance,
		const TCHAR* PropertyName,
		const FName Value,
		FString& OutError) const
	{
		check(OwnerClass != nullptr);
		check(ObjectInstance != nullptr);

		FNameProperty* const Property = CastField<FNameProperty>(OwnerClass->FindPropertyByName(*FString(PropertyName)));
		if (Property == nullptr)
		{
			OutError = FString::Printf(
				TEXT("Name property %s was not found on %s."),
				PropertyName,
				*OwnerClass->GetPathName());
			return false;
		}

		Property->SetPropertyValue_InContainer(ObjectInstance, Value);
		return true;
	}

	bool FOctoMCPModule::SetEnumPropertyValue(
		UClass* const OwnerClass,
		UObject* const ObjectInstance,
		const TCHAR* PropertyName,
		const int64 Value,
		FString& OutError) const
	{
		check(OwnerClass != nullptr);
		check(ObjectInstance != nullptr);

		if (FEnumProperty* const EnumProperty = CastField<FEnumProperty>(OwnerClass->FindPropertyByName(*FString(PropertyName))))
		{
			if (FNumericProperty* const UnderlyingProperty = EnumProperty->GetUnderlyingProperty())
			{
				UnderlyingProperty->SetIntPropertyValue(UnderlyingProperty->ContainerPtrToValuePtr<void>(ObjectInstance), Value);
				return true;
			}
		}

		if (FByteProperty* const ByteProperty = CastField<FByteProperty>(OwnerClass->FindPropertyByName(*FString(PropertyName))))
		{
			ByteProperty->SetPropertyValue_InContainer(ObjectInstance, static_cast<uint8>(Value));
			return true;
		}

		OutError = FString::Printf(
			TEXT("Enum property %s was not found on %s."),
			PropertyName,
			*OwnerClass->GetPathName());
		return false;
	}

	bool FOctoMCPModule::SetVector2DPropertyValue(
		UClass* const OwnerClass,
		UObject* const ObjectInstance,
		const TCHAR* PropertyName,
		const FVector2D& Value,
		FString& OutError) const
	{
		check(OwnerClass != nullptr);
		check(ObjectInstance != nullptr);

		FStructProperty* const Property = CastField<FStructProperty>(OwnerClass->FindPropertyByName(*FString(PropertyName)));
		if (Property == nullptr || Property->Struct != TBaseStructure<FVector2D>::Get())
		{
			OutError = FString::Printf(
				TEXT("FVector2D property %s was not found on %s."),
				PropertyName,
				*OwnerClass->GetPathName());
			return false;
		}

		if (FVector2D* const ValuePtr = Property->ContainerPtrToValuePtr<FVector2D>(ObjectInstance))
		{
			*ValuePtr = Value;
			return true;
		}

		OutError = FString::Printf(
			TEXT("Could not access FVector2D property %s on %s."),
			PropertyName,
			*OwnerClass->GetPathName());
		return false;
	}

	bool FOctoMCPModule::ParseOrientationValue(
		const FString& InOrientation,
		EOrientation& OutOrientation,
		FString& OutNormalizedOrientation,
		FString& OutError) const
	{
		const FString TrimmedOrientation = InOrientation.TrimStartAndEnd();
		if (TrimmedOrientation.IsEmpty())
		{
			OutError = TEXT("orientation must not be empty.");
			return false;
		}

		if (TrimmedOrientation.Equals(TEXT("Vertical"), ESearchCase::IgnoreCase) ||
			TrimmedOrientation.Equals(TEXT("Orient_Vertical"), ESearchCase::IgnoreCase))
		{
			OutOrientation = Orient_Vertical;
			OutNormalizedOrientation = TEXT("Vertical");
			return true;
		}

		if (TrimmedOrientation.Equals(TEXT("Horizontal"), ESearchCase::IgnoreCase) ||
			TrimmedOrientation.Equals(TEXT("Orient_Horizontal"), ESearchCase::IgnoreCase))
		{
			OutOrientation = Orient_Horizontal;
			OutNormalizedOrientation = TEXT("Horizontal");
			return true;
		}

		OutError = FString::Printf(
			TEXT("Unsupported orientation value %s. Expected Vertical or Horizontal."),
			*TrimmedOrientation);
		return false;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildImportTextureAssetObject(
		const FString& SourceFilePath,
		const FString& AssetPath,
		const bool bReplaceExisting,
		const bool bSaveAsset) const
	{
		const FImportTextureAssetResult ImportResult =
			ImportTextureAsset(SourceFilePath, AssetPath, bReplaceExisting, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("imported"), ImportResult.bImported);
		ResultObject->SetBoolField(TEXT("saved"), ImportResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), ImportResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), ImportResult.Message);
		ResultObject->SetStringField(TEXT("sourceFilePath"), ImportResult.SourceFilePath);
		ResultObject->SetStringField(TEXT("assetPath"), ImportResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), ImportResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), ImportResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), ImportResult.AssetName);
		return ResultObject;
	}

	FImportTextureAssetResult FOctoMCPModule::ImportTextureAsset(
		const FString& InSourceFilePath,
		const FString& InAssetPath,
		const bool bReplaceExisting,
		const bool bSaveAsset) const
	{
		FImportTextureAssetResult Result;

		Result.SourceFilePath = FPaths::ConvertRelativePathToFull(InSourceFilePath.TrimStartAndEnd());
		if (Result.SourceFilePath.IsEmpty())
		{
			Result.Message = TEXT("sourceFilePath must not be empty.");
			return Result;
		}

		if (!FPaths::FileExists(Result.SourceFilePath))
		{
			Result.Message = FString::Printf(TEXT("Source file does not exist: %s"), *Result.SourceFilePath);
			return Result;
		}

		FString AssetPackageName;
		FString AssetObjectPath;
		FString ErrorMessage;
		if (!NormalizeWidgetBlueprintAssetPath(
				InAssetPath,
				AssetPackageName,
				Result.PackagePath,
				Result.AssetName,
				AssetObjectPath,
				ErrorMessage))
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		Result.AssetPath = AssetPackageName;
		Result.AssetObjectPath = AssetObjectPath;
		UPackage* const TexturePackage = CreatePackage(*Result.AssetPath);
		if (TexturePackage == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Failed to create texture package: %s"), *Result.AssetPath);
			return Result;
		}

		TexturePackage->FullyLoad();

		UTexture2D* const ExistingTexture = FindObject<UTexture2D>(TexturePackage, *Result.AssetName);
		if (ExistingTexture != nullptr && !bReplaceExisting)
		{
			Result.Message = FString::Printf(TEXT("Texture asset already exists: %s"), *Result.AssetObjectPath);
			return Result;
		}

		TArray<uint8> SourceFileData;
		if (!FFileHelper::LoadFileToArray(SourceFileData, *Result.SourceFilePath) || SourceFileData.IsEmpty())
		{
			Result.Message = FString::Printf(TEXT("Failed to read source file: %s"), *Result.SourceFilePath);
			return Result;
		}

		const FString FileExtension = FPaths::GetExtension(Result.SourceFilePath);
		if (FileExtension.IsEmpty())
		{
			Result.Message = FString::Printf(TEXT("Could not determine file extension: %s"), *Result.SourceFilePath);
			return Result;
		}

		UTextureFactory* const TextureFactory = NewObject<UTextureFactory>();
		TextureFactory->AddToRoot();
		UTextureFactory::SuppressImportOverwriteDialog(bReplaceExisting);

		const uint8* TextureDataStart = SourceFileData.GetData();
		UObject* const ImportedObject = TextureFactory->FactoryCreateBinary(
			UTexture2D::StaticClass(),
			TexturePackage,
			*Result.AssetName,
			RF_Standalone | RF_Public,
			nullptr,
			*FileExtension,
			TextureDataStart,
			TextureDataStart + SourceFileData.Num(),
			GWarn);

		TextureFactory->RemoveFromRoot();

		UTexture2D* const ImportedTexture = Cast<UTexture2D>(ImportedObject);
		if (ImportedTexture == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Failed to import %s as a texture asset."),
				*Result.SourceFilePath);
			return Result;
		}

		Result.bImported = true;
		Result.AssetObjectPath = ImportedTexture->GetPathName();
		Result.AssetPath = ImportedTexture->GetOutermost()->GetName();
		Result.PackagePath = FPackageName::GetLongPackagePath(Result.AssetPath);
		Result.AssetName = FPackageName::GetLongPackageAssetName(Result.AssetPath);

		if (ImportedTexture->AssetImportData != nullptr)
		{
			ImportedTexture->AssetImportData->Update(
				IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Result.SourceFilePath));
		}

		if (ExistingTexture == nullptr)
		{
			FAssetRegistryModule::AssetCreated(ImportedTexture);
		}

		TexturePackage->MarkPackageDirty();

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Imported texture but could not access the EditorAssetSubsystem to save it: %s"),
					*Result.AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(ImportedTexture, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Imported texture but failed to save it: %s"),
					*Result.AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Imported texture %s from %s."),
			*Result.AssetObjectPath,
			*Result.SourceFilePath);
		return Result;
	}
