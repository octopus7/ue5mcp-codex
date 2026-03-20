// Copyright Epic Games, Inc. All Rights Reserved.

#include "OctoMCPModule.h"

	TSharedRef<FJsonObject> FOctoMCPModule::BuildSetWidgetImageTextureObject(
		const FString& AssetPath,
		const FString& WidgetName,
		const FString& TextureAssetPath,
		const bool bMatchTextureSize,
		const bool bSaveAsset) const
	{
		const FSetWidgetImageTextureResult SetResult =
			SetWidgetImageTexture(AssetPath, WidgetName, TextureAssetPath, bMatchTextureSize, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("saved"), SetResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), SetResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), SetResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), SetResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), SetResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), SetResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), SetResult.AssetName);
		ResultObject->SetStringField(TEXT("widgetName"), SetResult.WidgetName);
		ResultObject->SetStringField(TEXT("textureAssetPath"), SetResult.TextureAssetPath);
		return ResultObject;
	}

	FSetWidgetImageTextureResult FOctoMCPModule::SetWidgetImageTexture(
		const FString& InAssetPath,
		const FString& InWidgetName,
		const FString& InTextureAssetPath,
		const bool bMatchTextureSize,
		const bool bSaveAsset) const
	{
		FSetWidgetImageTextureResult Result;

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
		Result.WidgetName = InWidgetName.TrimStartAndEnd();
		if (Result.WidgetName.IsEmpty())
		{
			Result.Message = TEXT("widgetName must not be empty.");
			return Result;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		if (WidgetBlueprint->WidgetTree == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Widget Blueprint is missing a widget tree: %s"), *AssetObjectPath);
			return Result;
		}

		UTexture2D* const TextureAsset = ResolveTextureAsset(InTextureAssetPath, Result.TextureAssetPath, ErrorMessage);
		if (TextureAsset == nullptr)
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		UImage* const ImageWidget = Cast<UImage>(WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.WidgetName)));
		if (ImageWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not find UImage widget named %s in %s."),
				*Result.WidgetName,
				*AssetObjectPath);
			return Result;
		}

		WidgetBlueprint->Modify();
		ImageWidget->SetFlags(RF_Transactional);
		ImageWidget->Modify();
		ImageWidget->SetBrushFromTexture(TextureAsset, bMatchTextureSize);

		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Updated widget image texture but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Updated widget image texture but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Set widget image %s on %s to texture %s."),
			*Result.WidgetName,
			*AssetObjectPath,
			*Result.TextureAssetPath);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildSetWidgetBackgroundBlurObject(
		const FString& AssetPath,
		const FString& WidgetName,
		const float BlurStrength,
		const TOptional<int32> BlurRadius,
		const bool bApplyAlphaToBlur,
		const bool bSaveAsset) const
	{
		const FSetWidgetBackgroundBlurResult SetResult =
			SetWidgetBackgroundBlur(AssetPath, WidgetName, BlurStrength, BlurRadius, bApplyAlphaToBlur, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("converted"), SetResult.bConverted);
		ResultObject->SetBoolField(TEXT("saved"), SetResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), SetResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), SetResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), SetResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), SetResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), SetResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), SetResult.AssetName);
		ResultObject->SetStringField(TEXT("widgetName"), SetResult.WidgetName);
		ResultObject->SetStringField(TEXT("widgetClassName"), SetResult.WidgetClassName);
		ResultObject->SetBoolField(TEXT("applyAlphaToBlur"), SetResult.bApplyAlphaToBlur);
		ResultObject->SetBoolField(TEXT("overrideAutoRadiusCalculation"), SetResult.bOverrideAutoRadiusCalculation);
		ResultObject->SetNumberField(TEXT("blurStrength"), SetResult.BlurStrength);
		ResultObject->SetNumberField(TEXT("blurRadius"), SetResult.BlurRadius);
		return ResultObject;
	}

	FSetWidgetBackgroundBlurResult FOctoMCPModule::SetWidgetBackgroundBlur(
		const FString& InAssetPath,
		const FString& InWidgetName,
		const float InBlurStrength,
		const TOptional<int32> InBlurRadius,
		const bool bApplyAlphaToBlur,
		const bool bSaveAsset) const
	{
		FSetWidgetBackgroundBlurResult Result;

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
		Result.WidgetName = InWidgetName.TrimStartAndEnd();

		if (Result.WidgetName.IsEmpty())
		{
			Result.Message = TEXT("widgetName must not be empty.");
			return Result;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		UWidget* const ExistingWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.WidgetName));
		if (ExistingWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not find widget named %s in %s."),
				*Result.WidgetName,
				*AssetObjectPath);
			return Result;
		}

		UBackgroundBlur* BackgroundBlurWidget = Cast<UBackgroundBlur>(ExistingWidget);
		if (BackgroundBlurWidget == nullptr)
		{
			if (!ExistingWidget->IsA<UContentWidget>())
			{
				Result.Message = FString::Printf(
					TEXT("Widget %s must be a content widget to convert it into a background blur."),
					*Result.WidgetName);
				return Result;
			}

			if (ExistingWidget->bIsVariable && FBlueprintEditorUtils::IsVariableUsed(WidgetBlueprint, ExistingWidget->GetFName()))
			{
				Result.Message = FString::Printf(
					TEXT("Widget %s is referenced in the graph and cannot be converted automatically."),
					*Result.WidgetName);
				return Result;
			}

			TSet<UWidget*> WidgetsToReplace;
			WidgetsToReplace.Add(ExistingWidget);
			FWidgetBlueprintEditorUtils::ReplaceWidgets(
				WidgetBlueprint,
				WidgetsToReplace,
				UBackgroundBlur::StaticClass(),
				FWidgetBlueprintEditorUtils::EReplaceWidgetNamingMethod::MaintainNameAndReferences);

			BackgroundBlurWidget = FindWidgetByName<UBackgroundBlur>(WidgetBlueprint, *Result.WidgetName);
			if (BackgroundBlurWidget == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Failed to convert widget %s into a background blur in %s."),
					*Result.WidgetName,
					*AssetObjectPath);
				return Result;
			}

			Result.bConverted = true;
		}

		Result.BlurStrength = FMath::Clamp(InBlurStrength, 0.0f, 100.0f);
		Result.bApplyAlphaToBlur = bApplyAlphaToBlur;
		Result.bOverrideAutoRadiusCalculation = InBlurRadius.IsSet();
		Result.BlurRadius = FMath::Clamp(InBlurRadius.Get(0), 0, 255);
		Result.WidgetClassName = BackgroundBlurWidget->GetClass()->GetName();

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		BackgroundBlurWidget->SetFlags(RF_Transactional);
		BackgroundBlurWidget->Modify();
		BackgroundBlurWidget->SetApplyAlphaToBlur(Result.bApplyAlphaToBlur);
		BackgroundBlurWidget->SetBlurStrength(Result.BlurStrength);
		BackgroundBlurWidget->SetOverrideAutoRadiusCalculation(Result.bOverrideAutoRadiusCalculation);
		if (Result.bOverrideAutoRadiusCalculation)
		{
			BackgroundBlurWidget->SetBlurRadius(Result.BlurRadius);
		}

		WidgetBlueprint->MarkPackageDirty();
		if (Result.bConverted)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
		}
		else
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		}
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Updated widget background blur but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Updated widget background blur but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Configured widget %s on %s as a background blur."),
			*Result.WidgetName,
			*AssetObjectPath);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildSetWidgetCornerRadiusObject(
		const FString& AssetPath,
		const FString& WidgetName,
		const float Radius,
		const bool bSaveAsset) const
	{
		const FSetWidgetCornerRadiusResult SetResult =
			SetWidgetCornerRadius(AssetPath, WidgetName, Radius, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("saved"), SetResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), SetResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), SetResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), SetResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), SetResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), SetResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), SetResult.AssetName);
		ResultObject->SetStringField(TEXT("widgetName"), SetResult.WidgetName);
		ResultObject->SetStringField(TEXT("widgetClassName"), SetResult.WidgetClassName);
		ResultObject->SetNumberField(TEXT("radius"), SetResult.Radius);
		return ResultObject;
	}

	FSetWidgetCornerRadiusResult FOctoMCPModule::SetWidgetCornerRadius(
		const FString& InAssetPath,
		const FString& InWidgetName,
		const float InRadius,
		const bool bSaveAsset) const
	{
		FSetWidgetCornerRadiusResult Result;

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
		Result.WidgetName = InWidgetName.TrimStartAndEnd();
		Result.Radius = FMath::Max(0.0f, InRadius);

		if (Result.WidgetName.IsEmpty())
		{
			Result.Message = TEXT("widgetName must not be empty.");
			return Result;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		UWidget* const TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.WidgetName));
		if (TargetWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not find widget named %s in %s."),
				*Result.WidgetName,
				*AssetObjectPath);
			return Result;
		}

		const FVector4 CornerRadius(Result.Radius, Result.Radius, Result.Radius, Result.Radius);

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		TargetWidget->SetFlags(RF_Transactional);
		TargetWidget->Modify();

		if (UBackgroundBlur* const BackgroundBlurWidget = Cast<UBackgroundBlur>(TargetWidget))
		{
			BackgroundBlurWidget->SetCornerRadius(CornerRadius);
			Result.WidgetClassName = BackgroundBlurWidget->GetClass()->GetName();
		}
		else if (UBorder* const BorderWidget = Cast<UBorder>(TargetWidget))
		{
			FSlateBrush RoundedBrush = BorderWidget->Background;
			RoundedBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			RoundedBrush.Tiling = ESlateBrushTileType::NoTile;
			RoundedBrush.Margin = FMargin(0.0f);
			RoundedBrush.OutlineSettings = FSlateBrushOutlineSettings(CornerRadius);
			BorderWidget->SetBrush(RoundedBrush);
			Result.WidgetClassName = BorderWidget->GetClass()->GetName();
		}
		else
		{
			Result.Message = FString::Printf(
				TEXT("Widget %s does not support corner radius. Supported widget classes are BackgroundBlur and Border."),
				*Result.WidgetName);
			return Result;
		}

		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Updated widget corner radius but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Updated widget corner radius but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Set corner radius on widget %s in %s to %.2f."),
			*Result.WidgetName,
			*AssetObjectPath,
			Result.Radius);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildSetWidgetPanelColorObject(
		const FString& AssetPath,
		const FString& WidgetName,
		const float Red,
		const float Green,
		const float Blue,
		const float Alpha,
		const bool bSaveAsset) const
	{
		const FSetWidgetPanelColorResult SetResult =
			SetWidgetPanelColor(AssetPath, WidgetName, Red, Green, Blue, Alpha, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("saved"), SetResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), SetResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), SetResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), SetResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), SetResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), SetResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), SetResult.AssetName);
		ResultObject->SetStringField(TEXT("widgetName"), SetResult.WidgetName);
		ResultObject->SetStringField(TEXT("widgetClassName"), SetResult.WidgetClassName);
		ResultObject->SetNumberField(TEXT("red"), SetResult.Red);
		ResultObject->SetNumberField(TEXT("green"), SetResult.Green);
		ResultObject->SetNumberField(TEXT("blue"), SetResult.Blue);
		ResultObject->SetNumberField(TEXT("alpha"), SetResult.Alpha);
		return ResultObject;
	}

	FSetWidgetPanelColorResult FOctoMCPModule::SetWidgetPanelColor(
		const FString& InAssetPath,
		const FString& InWidgetName,
		const float InRed,
		const float InGreen,
		const float InBlue,
		const float InAlpha,
		const bool bSaveAsset) const
	{
		FSetWidgetPanelColorResult Result;

		auto NormalizeColorChannel = [](float Value) -> float
		{
			const float NormalizedValue = Value > 1.0f ? Value / 255.0f : Value;
			return FMath::Clamp(NormalizedValue, 0.0f, 1.0f);
		};

		Result.Red = NormalizeColorChannel(InRed);
		Result.Green = NormalizeColorChannel(InGreen);
		Result.Blue = NormalizeColorChannel(InBlue);
		Result.Alpha = NormalizeColorChannel(InAlpha);

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
		Result.WidgetName = InWidgetName.TrimStartAndEnd();

		if (Result.WidgetName.IsEmpty())
		{
			Result.Message = TEXT("widgetName must not be empty.");
			return Result;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		UWidget* const TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.WidgetName));
		if (TargetWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not find widget named %s in %s."),
				*Result.WidgetName,
				*AssetObjectPath);
			return Result;
		}

		const FLinearColor PanelColor(Result.Red, Result.Green, Result.Blue, Result.Alpha);

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		TargetWidget->SetFlags(RF_Transactional);
		TargetWidget->Modify();

		if (UBorder* const BorderWidget = Cast<UBorder>(TargetWidget))
		{
			BorderWidget->SetBrushColor(PanelColor);
			Result.WidgetClassName = BorderWidget->GetClass()->GetName();
		}
		else if (UButton* const ButtonWidget = Cast<UButton>(TargetWidget))
		{
			ButtonWidget->SetBackgroundColor(PanelColor);
			Result.WidgetClassName = ButtonWidget->GetClass()->GetName();
		}
		else if (UImage* const ImageWidget = Cast<UImage>(TargetWidget))
		{
			ImageWidget->SetColorAndOpacity(PanelColor);
			Result.WidgetClassName = ImageWidget->GetClass()->GetName();
		}
		else
		{
			Result.Message = FString::Printf(
				TEXT("Widget %s does not support direct RGBA panel color updates. Supported widget classes are Border, Button, and Image."),
				*Result.WidgetName);
			return Result;
		}

		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Updated widget panel color but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Updated widget panel color but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Set widget %s on %s to RGBA(%.3f, %.3f, %.3f, %.3f)."),
			*Result.WidgetName,
			*AssetObjectPath,
			Result.Red,
			Result.Green,
			Result.Blue,
			Result.Alpha);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildSetSizeBoxHeightOverrideObject(
		const FString& AssetPath,
		const FString& WidgetName,
		const float HeightOverride,
		const bool bSaveAsset) const
	{
		const FSetSizeBoxHeightOverrideResult SetResult =
			SetSizeBoxHeightOverride(AssetPath, WidgetName, HeightOverride, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("saved"), SetResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), SetResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), SetResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), SetResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), SetResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), SetResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), SetResult.AssetName);
		ResultObject->SetStringField(TEXT("widgetName"), SetResult.WidgetName);
		ResultObject->SetNumberField(TEXT("heightOverride"), SetResult.HeightOverride);
		return ResultObject;
	}

	FSetSizeBoxHeightOverrideResult FOctoMCPModule::SetSizeBoxHeightOverride(
		const FString& InAssetPath,
		const FString& InWidgetName,
		const float InHeightOverride,
		const bool bSaveAsset) const
	{
		FSetSizeBoxHeightOverrideResult Result;
		Result.HeightOverride = FMath::Max(0.0f, InHeightOverride);

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
		Result.WidgetName = InWidgetName.TrimStartAndEnd();

		if (Result.WidgetName.IsEmpty())
		{
			Result.Message = TEXT("widgetName must not be empty.");
			return Result;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		USizeBox* const SizeBoxWidget = Cast<USizeBox>(WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.WidgetName)));
		if (SizeBoxWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not find USizeBox widget named %s in %s."),
				*Result.WidgetName,
				*AssetObjectPath);
			return Result;
		}

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		SizeBoxWidget->SetFlags(RF_Transactional);
		SizeBoxWidget->Modify();
		SizeBoxWidget->SetHeightOverride(Result.HeightOverride);

		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Updated size box height override but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Updated size box height override but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Set height override on size box %s in %s to %.2f."),
			*Result.WidgetName,
			*AssetObjectPath,
			Result.HeightOverride);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildSetPopupOpenElasticScaleObject(
		const FString& AssetPath,
		const bool bEnabled,
		const FString& WidgetName,
		const float Duration,
		const float StartScale,
		const float OscillationCount,
		const float PivotX,
		const float PivotY,
		const bool bSaveAsset) const
	{
		const FSetPopupOpenElasticScaleResult SetResult =
			SetPopupOpenElasticScale(AssetPath, bEnabled, WidgetName, Duration, StartScale, OscillationCount, PivotX, PivotY, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("saved"), SetResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), SetResult.bSuccess);
		ResultObject->SetBoolField(TEXT("enabled"), SetResult.bEnabled);
		ResultObject->SetStringField(TEXT("message"), SetResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), SetResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), SetResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), SetResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), SetResult.AssetName);
		ResultObject->SetStringField(TEXT("widgetName"), SetResult.WidgetName);
		ResultObject->SetNumberField(TEXT("duration"), SetResult.Duration);
		ResultObject->SetNumberField(TEXT("startScale"), SetResult.StartScale);
		ResultObject->SetNumberField(TEXT("oscillationCount"), SetResult.OscillationCount);
		ResultObject->SetNumberField(TEXT("pivotX"), SetResult.PivotX);
		ResultObject->SetNumberField(TEXT("pivotY"), SetResult.PivotY);
		return ResultObject;
	}

	FSetPopupOpenElasticScaleResult FOctoMCPModule::SetPopupOpenElasticScale(
		const FString& InAssetPath,
		const bool bEnabled,
		const FString& InWidgetName,
		const float InDuration,
		const float InStartScale,
		const float InOscillationCount,
		const float InPivotX,
		const float InPivotY,
		const bool bSaveAsset) const
	{
		FSetPopupOpenElasticScaleResult Result;
		Result.bEnabled = bEnabled;
		Result.Duration = FMath::Max(InDuration, 0.05f);
		Result.StartScale = FMath::Clamp(InStartScale, 0.10f, 1.00f);
		Result.OscillationCount = FMath::Max(InOscillationCount, 0.50f);
		Result.PivotX = FMath::Clamp(InPivotX, 0.0f, 1.0f);
		Result.PivotY = FMath::Clamp(InPivotY, 0.0f, 1.0f);

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
		Result.WidgetName = InWidgetName.TrimStartAndEnd();
		if (Result.WidgetName.IsEmpty())
		{
			Result.WidgetName = TEXT("PopupCard");
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
		if (WidgetBlueprint->GeneratedClass == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Widget Blueprint asset does not have a generated class after compile: %s"),
				*AssetObjectPath);
			return Result;
		}

		FString PopupWidgetClassPath;
		UClass* const PopupWidgetBaseClass =
			ResolveClassReference(TEXT("UMCPPopupWidget"), UUserWidget::StaticClass(), PopupWidgetClassPath, ErrorMessage);
		if (PopupWidgetBaseClass == nullptr)
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		if (!WidgetBlueprint->GeneratedClass->IsChildOf(PopupWidgetBaseClass))
		{
			Result.Message = FString::Printf(
				TEXT("Widget Blueprint %s does not derive from %s."),
				*AssetObjectPath,
				*PopupWidgetClassPath);
			return Result;
		}

		UObject* const BlueprintDefaultObject = WidgetBlueprint->GeneratedClass->GetDefaultObject();
		if (BlueprintDefaultObject == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Widget Blueprint generated class does not have a default object: %s"),
				*WidgetBlueprint->GeneratedClass->GetPathName());
			return Result;
		}

		WidgetBlueprint->Modify();
		BlueprintDefaultObject->SetFlags(RF_Transactional);
		BlueprintDefaultObject->Modify();

		if (!SetBoolPropertyValue(
				WidgetBlueprint->GeneratedClass,
				BlueprintDefaultObject,
				TEXT("bPlayOpenElasticScaleAnimation"),
				Result.bEnabled,
				ErrorMessage) ||
			!SetFloatPropertyValue(
				WidgetBlueprint->GeneratedClass,
				BlueprintDefaultObject,
				TEXT("OpenElasticScaleDuration"),
				Result.Duration,
				ErrorMessage) ||
			!SetFloatPropertyValue(
				WidgetBlueprint->GeneratedClass,
				BlueprintDefaultObject,
				TEXT("OpenElasticStartScale"),
				Result.StartScale,
				ErrorMessage) ||
			!SetFloatPropertyValue(
				WidgetBlueprint->GeneratedClass,
				BlueprintDefaultObject,
				TEXT("OpenElasticOscillationCount"),
				Result.OscillationCount,
				ErrorMessage) ||
			!SetNamePropertyValue(
				WidgetBlueprint->GeneratedClass,
				BlueprintDefaultObject,
				TEXT("OpenElasticTargetWidgetName"),
				FName(*Result.WidgetName),
				ErrorMessage) ||
			!SetVector2DPropertyValue(
				WidgetBlueprint->GeneratedClass,
				BlueprintDefaultObject,
				TEXT("OpenElasticPivot"),
				FVector2D(Result.PivotX, Result.PivotY),
				ErrorMessage))
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Updated popup open elastic scale settings but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Updated popup open elastic scale settings but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Configured popup open elastic scale on %s for widget %s."),
			*AssetObjectPath,
			*Result.WidgetName);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildAddWidgetBlueprintChildInstanceObject(
		const FString& AssetPath,
		const FString& ParentWidgetName,
		const FString& ChildWidgetAssetPath,
		const FString& ChildWidgetName,
		const int32 DesiredIndex,
		const bool bSaveAsset) const
	{
		const FAddWidgetBlueprintChildInstanceResult AddResult =
			AddWidgetBlueprintChildInstance(AssetPath, ParentWidgetName, ChildWidgetAssetPath, ChildWidgetName, DesiredIndex, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("created"), AddResult.bCreated);
		ResultObject->SetBoolField(TEXT("replaced"), AddResult.bReplaced);
		ResultObject->SetBoolField(TEXT("saved"), AddResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), AddResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), AddResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), AddResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), AddResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), AddResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), AddResult.AssetName);
		ResultObject->SetStringField(TEXT("parentWidgetName"), AddResult.ParentWidgetName);
		ResultObject->SetStringField(TEXT("childWidgetName"), AddResult.ChildWidgetName);
		ResultObject->SetStringField(TEXT("childWidgetAssetPath"), AddResult.ChildWidgetAssetPath);
		ResultObject->SetStringField(TEXT("childWidgetClassPath"), AddResult.ChildWidgetClassPath);
		ResultObject->SetStringField(TEXT("childWidgetClassName"), AddResult.ChildWidgetClassName);
		ResultObject->SetNumberField(TEXT("finalIndex"), AddResult.FinalIndex);
		return ResultObject;
	}

	FAddWidgetBlueprintChildInstanceResult FOctoMCPModule::AddWidgetBlueprintChildInstance(
		const FString& InAssetPath,
		const FString& InParentWidgetName,
		const FString& InChildWidgetAssetPath,
		const FString& InChildWidgetName,
		const int32 DesiredIndex,
		const bool bSaveAsset) const
	{
		FAddWidgetBlueprintChildInstanceResult Result;

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
		Result.ParentWidgetName = InParentWidgetName.TrimStartAndEnd();
		Result.ChildWidgetName = InChildWidgetName.TrimStartAndEnd();

		if (Result.ParentWidgetName.IsEmpty())
		{
			Result.Message = TEXT("parentWidgetName must not be empty.");
			return Result;
		}

		if (Result.ChildWidgetName.IsEmpty())
		{
			Result.Message = TEXT("childWidgetName must not be empty.");
			return Result;
		}

		if (DesiredIndex < 0)
		{
			Result.Message = TEXT("desiredIndex must be zero or greater.");
			return Result;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		UPanelWidget* const ParentWidget =
			Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.ParentWidgetName)));
		if (ParentWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not find panel widget named %s in %s."),
				*Result.ParentWidgetName,
				*AssetObjectPath);
			return Result;
		}

		UClass* const ChildWidgetClass = ResolveWidgetBlueprintGeneratedClass(
			InChildWidgetAssetPath,
			Result.ChildWidgetAssetPath,
			Result.ChildWidgetClassPath,
			ErrorMessage);
		if (ChildWidgetClass == nullptr)
		{
			Result.Message = ErrorMessage;
			return Result;
		}
		Result.ChildWidgetClassName = ChildWidgetClass->GetName();

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		ParentWidget->SetFlags(RF_Transactional);
		ParentWidget->Modify();

		UWidget* const ChildWidget = EnsureWidgetInstanceOfClass(
			WidgetBlueprint,
			ChildWidgetClass,
			*Result.ChildWidgetName,
			Result.bCreated,
			Result.bReplaced,
			ErrorMessage);
		if (ChildWidget == nullptr)
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		ChildWidget->SetFlags(RF_Transactional);
		ChildWidget->Modify();

		if (EnsurePanelChildAt(ParentWidget, ChildWidget, DesiredIndex) == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Failed to insert child widget %s under parent %s."),
				*Result.ChildWidgetName,
				*Result.ParentWidgetName);
			return Result;
		}

		Result.FinalIndex = ParentWidget->GetChildIndex(ChildWidget);
		if (Result.FinalIndex == INDEX_NONE)
		{
			Result.Message = FString::Printf(
				TEXT("Failed to resolve final child index for widget %s under parent %s."),
				*Result.ChildWidgetName,
				*Result.ParentWidgetName);
			return Result;
		}

		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Added widget child instance but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Added widget child instance but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Placed child widget %s under %s at index %d using class %s."),
			*Result.ChildWidgetName,
			*Result.ParentWidgetName,
			Result.FinalIndex,
			*Result.ChildWidgetClassPath);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildSetUniformGridSlotObject(
		const FString& AssetPath,
		const FString& WidgetName,
		const int32 Row,
		const int32 Column,
		const bool bSaveAsset) const
	{
		const FSetUniformGridSlotResult SetResult =
			SetUniformGridSlot(AssetPath, WidgetName, Row, Column, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("saved"), SetResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), SetResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), SetResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), SetResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), SetResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), SetResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), SetResult.AssetName);
		ResultObject->SetStringField(TEXT("widgetName"), SetResult.WidgetName);
		ResultObject->SetStringField(TEXT("parentWidgetName"), SetResult.ParentWidgetName);
		ResultObject->SetNumberField(TEXT("row"), SetResult.Row);
		ResultObject->SetNumberField(TEXT("column"), SetResult.Column);
		return ResultObject;
	}

	FSetUniformGridSlotResult FOctoMCPModule::SetUniformGridSlot(
		const FString& InAssetPath,
		const FString& InWidgetName,
		const int32 InRow,
		const int32 InColumn,
		const bool bSaveAsset) const
	{
		FSetUniformGridSlotResult Result;
		Result.Row = FMath::Max(InRow, 0);
		Result.Column = FMath::Max(InColumn, 0);

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
		Result.WidgetName = InWidgetName.TrimStartAndEnd();
		if (Result.WidgetName.IsEmpty())
		{
			Result.Message = TEXT("widgetName must not be empty.");
			return Result;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		UWidget* const TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.WidgetName));
		if (TargetWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not find widget named %s in %s."),
				*Result.WidgetName,
				*AssetObjectPath);
			return Result;
		}

		UUniformGridSlot* const GridSlot = Cast<UUniformGridSlot>(TargetWidget->Slot);
		if (GridSlot == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Widget %s is not currently slotted in a UniformGridPanel."),
				*Result.WidgetName);
			return Result;
		}

		if (UUniformGridPanel* const ParentWidget = Cast<UUniformGridPanel>(GridSlot->Parent))
		{
			Result.ParentWidgetName = ParentWidget->GetName();
		}

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		GridSlot->SetFlags(RF_Transactional);
		GridSlot->Modify();
		GridSlot->SetRow(Result.Row);
		GridSlot->SetColumn(Result.Column);

		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Updated UniformGrid slot but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Updated UniformGrid slot but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Set UniformGrid slot for widget %s to row=%d column=%d."),
			*Result.WidgetName,
			Result.Row,
			Result.Column);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildSyncUniformGridWidgetInstancesObject(
		const FString& AssetPath,
		const FString& GridWidgetName,
		const FString& EntryWidgetAssetPath,
		const int32 Count,
		const int32 ColumnCount,
		const FString& InstanceNamePrefix,
		const bool bTrimManagedChildren,
		const bool bSaveAsset) const
	{
		const FSyncUniformGridWidgetInstancesResult SyncResult = SyncUniformGridWidgetInstances(
			AssetPath,
			GridWidgetName,
			EntryWidgetAssetPath,
			Count,
			ColumnCount,
			InstanceNamePrefix,
			bTrimManagedChildren,
			bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("saved"), SyncResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), SyncResult.bSuccess);
		ResultObject->SetBoolField(TEXT("trimManagedChildren"), SyncResult.bTrimManagedChildren);
		ResultObject->SetStringField(TEXT("message"), SyncResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), SyncResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), SyncResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), SyncResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), SyncResult.AssetName);
		ResultObject->SetStringField(TEXT("gridWidgetName"), SyncResult.GridWidgetName);
		ResultObject->SetStringField(TEXT("entryWidgetAssetPath"), SyncResult.EntryWidgetAssetPath);
		ResultObject->SetStringField(TEXT("entryWidgetClassPath"), SyncResult.EntryWidgetClassPath);
		ResultObject->SetStringField(TEXT("entryWidgetClassName"), SyncResult.EntryWidgetClassName);
		ResultObject->SetStringField(TEXT("instanceNamePrefix"), SyncResult.InstanceNamePrefix);
		ResultObject->SetNumberField(TEXT("count"), SyncResult.Count);
		ResultObject->SetNumberField(TEXT("columnCount"), SyncResult.ColumnCount);
		ResultObject->SetNumberField(TEXT("createdCount"), SyncResult.CreatedCount);
		ResultObject->SetNumberField(TEXT("reusedCount"), SyncResult.ReusedCount);
		ResultObject->SetNumberField(TEXT("removedCount"), SyncResult.RemovedCount);
		ResultObject->SetNumberField(TEXT("managedCount"), SyncResult.ManagedCount);
		return ResultObject;
	}

	FSyncUniformGridWidgetInstancesResult FOctoMCPModule::SyncUniformGridWidgetInstances(
		const FString& InAssetPath,
		const FString& InGridWidgetName,
		const FString& InEntryWidgetAssetPath,
		const int32 InCount,
		const int32 InColumnCount,
		const FString& InInstanceNamePrefix,
		const bool bTrimManagedChildren,
		const bool bSaveAsset) const
	{
		FSyncUniformGridWidgetInstancesResult Result;
		Result.Count = FMath::Max(InCount, 0);
		Result.ColumnCount = FMath::Max(InColumnCount, 1);
		Result.bTrimManagedChildren = bTrimManagedChildren;

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
		Result.GridWidgetName = InGridWidgetName.TrimStartAndEnd();
		Result.InstanceNamePrefix = InInstanceNamePrefix.TrimStartAndEnd();
		if (Result.GridWidgetName.IsEmpty())
		{
			Result.Message = TEXT("gridWidgetName must not be empty.");
			return Result;
		}

		if (Result.InstanceNamePrefix.IsEmpty())
		{
			Result.Message = TEXT("instanceNamePrefix must not be empty.");
			return Result;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		UUniformGridPanel* const GridWidget =
			Cast<UUniformGridPanel>(WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.GridWidgetName)));
		if (GridWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not find UUniformGridPanel named %s in %s."),
				*Result.GridWidgetName,
				*AssetObjectPath);
			return Result;
		}

		UClass* const EntryWidgetClass = ResolveWidgetBlueprintGeneratedClass(
			InEntryWidgetAssetPath,
			Result.EntryWidgetAssetPath,
			Result.EntryWidgetClassPath,
			ErrorMessage);
		if (EntryWidgetClass == nullptr)
		{
			Result.Message = ErrorMessage;
			return Result;
		}
		Result.EntryWidgetClassName = EntryWidgetClass->GetName();

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		GridWidget->SetFlags(RF_Transactional);
		GridWidget->Modify();

		TSet<FString> DesiredChildNames;
		for (int32 Index = 0; Index < Result.Count; ++Index)
		{
			const FString DesiredWidgetName = FString::Printf(TEXT("%s%d"), *Result.InstanceNamePrefix, Index);
			DesiredChildNames.Add(DesiredWidgetName);

			bool bCreatedChild = false;
			bool bReplacedChild = false;
			UWidget* const ChildWidget = EnsureWidgetInstanceOfClass(
				WidgetBlueprint,
				EntryWidgetClass,
				*DesiredWidgetName,
				bCreatedChild,
				bReplacedChild,
				ErrorMessage);
			if (ChildWidget == nullptr)
			{
				Result.Message = ErrorMessage;
				return Result;
			}

			ChildWidget->SetFlags(RF_Transactional);
			ChildWidget->Modify();

			UUniformGridSlot* const GridSlot = Cast<UUniformGridSlot>(EnsurePanelChildAt(GridWidget, ChildWidget, Index));
			if (GridSlot == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Failed to insert widget %s into UniformGridPanel %s."),
					*DesiredWidgetName,
					*Result.GridWidgetName);
				return Result;
			}

			GridSlot->SetRow(Index / Result.ColumnCount);
			GridSlot->SetColumn(Index % Result.ColumnCount);

			if (bCreatedChild || bReplacedChild)
			{
				++Result.CreatedCount;
			}
			else
			{
				++Result.ReusedCount;
			}
		}

		if (bTrimManagedChildren)
		{
			TSet<UWidget*> WidgetsToDelete;
			for (int32 ChildIndex = 0; ChildIndex < GridWidget->GetChildrenCount(); ++ChildIndex)
			{
				UWidget* const ChildWidget = GridWidget->GetChildAt(ChildIndex);
				if (ChildWidget == nullptr)
				{
					continue;
				}

				const FString ChildWidgetName = ChildWidget->GetName();
				if (ChildWidgetName.StartsWith(Result.InstanceNamePrefix) && !DesiredChildNames.Contains(ChildWidgetName))
				{
					WidgetsToDelete.Add(ChildWidget);
				}
			}

			if (WidgetsToDelete.Num() > 0)
			{
				Result.RemovedCount = WidgetsToDelete.Num();
				FWidgetBlueprintEditorUtils::DeleteWidgets(
					WidgetBlueprint,
					WidgetsToDelete,
					FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);
			}
		}

		for (int32 ChildIndex = 0; ChildIndex < GridWidget->GetChildrenCount(); ++ChildIndex)
		{
			if (UWidget* const ChildWidget = GridWidget->GetChildAt(ChildIndex))
			{
				if (ChildWidget->GetName().StartsWith(Result.InstanceNamePrefix))
				{
					++Result.ManagedCount;
				}
			}
		}

		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Synchronized UniformGrid widget instances but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Synchronized UniformGrid widget instances but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Synchronized %d managed widget instances under UniformGridPanel %s using prefix %s."),
			Result.ManagedCount,
			*Result.GridWidgetName,
			*Result.InstanceNamePrefix);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildAddBlueprintInterfaceObject(
		const FString& AssetPath,
		const FString& InterfaceClassPath,
		const bool bSaveAsset) const
	{
		const FAddBlueprintInterfaceResult AddResult =
			AddBlueprintInterface(AssetPath, InterfaceClassPath, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("added"), AddResult.bAdded);
		ResultObject->SetBoolField(TEXT("saved"), AddResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), AddResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), AddResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), AddResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), AddResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), AddResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), AddResult.AssetName);
		ResultObject->SetStringField(TEXT("interfaceClassPath"), AddResult.InterfaceClassPath);
		ResultObject->SetStringField(TEXT("interfaceClassName"), AddResult.InterfaceClassName);
		return ResultObject;
	}

	FAddBlueprintInterfaceResult FOctoMCPModule::AddBlueprintInterface(
		const FString& InAssetPath,
		const FString& InInterfaceClassPath,
		const bool bSaveAsset) const
	{
		FAddBlueprintInterfaceResult Result;

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

		UBlueprint* const BlueprintAsset = LoadObject<UBlueprint>(nullptr, *AssetObjectPath);
		if (BlueprintAsset == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		UClass* const InterfaceClass =
			ResolveClassReference(InInterfaceClassPath, UInterface::StaticClass(), Result.InterfaceClassPath, ErrorMessage);
		if (InterfaceClass == nullptr)
		{
			Result.Message = ErrorMessage;
			return Result;
		}
		Result.InterfaceClassName = InterfaceClass->GetName();

		for (const FBPInterfaceDescription& ImplementedInterface : BlueprintAsset->ImplementedInterfaces)
		{
			if (ImplementedInterface.Interface == InterfaceClass)
			{
				Result.bSuccess = true;
				Result.Message = FString::Printf(
					TEXT("Blueprint %s already implements interface %s."),
					*AssetObjectPath,
					*Result.InterfaceClassPath);
				return Result;
			}
		}

		BlueprintAsset->Modify();
		if (!FBlueprintEditorUtils::ImplementNewInterface(BlueprintAsset, InterfaceClass->GetClassPathName()))
		{
			Result.Message = FString::Printf(
				TEXT("Failed to implement interface %s on %s."),
				*Result.InterfaceClassPath,
				*AssetObjectPath);
			return Result;
		}

		Result.bAdded = true;
		BlueprintAsset->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BlueprintAsset);
		FKismetEditorUtilities::CompileBlueprint(BlueprintAsset);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Added Blueprint interface but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(BlueprintAsset, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Added Blueprint interface but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Implemented interface %s on Blueprint %s."),
			*Result.InterfaceClassPath,
			*AssetObjectPath);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildConfigureTileViewObject(
		const FString& AssetPath,
		const FString& WidgetName,
		const FString& EntryWidgetAssetPath,
		const float EntryWidth,
		const float EntryHeight,
		const FString& Orientation,
		const bool bSaveAsset) const
	{
		const FConfigureTileViewResult ConfigureResult =
			ConfigureTileView(AssetPath, WidgetName, EntryWidgetAssetPath, EntryWidth, EntryHeight, Orientation, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("saved"), ConfigureResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), ConfigureResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), ConfigureResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), ConfigureResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), ConfigureResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), ConfigureResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), ConfigureResult.AssetName);
		ResultObject->SetStringField(TEXT("widgetName"), ConfigureResult.WidgetName);
		ResultObject->SetStringField(TEXT("entryWidgetAssetPath"), ConfigureResult.EntryWidgetAssetPath);
		ResultObject->SetStringField(TEXT("entryWidgetClassPath"), ConfigureResult.EntryWidgetClassPath);
		ResultObject->SetStringField(TEXT("entryWidgetClassName"), ConfigureResult.EntryWidgetClassName);
		ResultObject->SetStringField(TEXT("orientation"), ConfigureResult.Orientation);
		ResultObject->SetNumberField(TEXT("entryWidth"), ConfigureResult.EntryWidth);
		ResultObject->SetNumberField(TEXT("entryHeight"), ConfigureResult.EntryHeight);
		return ResultObject;
	}

	FConfigureTileViewResult FOctoMCPModule::ConfigureTileView(
		const FString& InAssetPath,
		const FString& InWidgetName,
		const FString& InEntryWidgetAssetPath,
		const float InEntryWidth,
		const float InEntryHeight,
		const FString& InOrientation,
		const bool bSaveAsset) const
	{
		FConfigureTileViewResult Result;
		Result.EntryWidth = FMath::Max(InEntryWidth, 1.0f);
		Result.EntryHeight = FMath::Max(InEntryHeight, 1.0f);

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
		Result.WidgetName = InWidgetName.TrimStartAndEnd();
		if (Result.WidgetName.IsEmpty())
		{
			Result.Message = TEXT("widgetName must not be empty.");
			return Result;
		}

		EOrientation OrientationValue = Orient_Vertical;
		if (!ParseOrientationValue(InOrientation, OrientationValue, Result.Orientation, ErrorMessage))
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		UTileView* const TileViewWidget = Cast<UTileView>(WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.WidgetName)));
		if (TileViewWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not find UTileView widget named %s in %s."),
				*Result.WidgetName,
				*AssetObjectPath);
			return Result;
		}

		UClass* const EntryWidgetClass = ResolveWidgetBlueprintGeneratedClass(
			InEntryWidgetAssetPath,
			Result.EntryWidgetAssetPath,
			Result.EntryWidgetClassPath,
			ErrorMessage);
		if (EntryWidgetClass == nullptr)
		{
			Result.Message = ErrorMessage;
			return Result;
		}
		Result.EntryWidgetClassName = EntryWidgetClass->GetName();

		if (!EntryWidgetClass->ImplementsInterface(UUserObjectListEntry::StaticClass()))
		{
			Result.Message = FString::Printf(
				TEXT("Entry widget class %s must implement /Script/UMG.UserObjectListEntry."),
				*Result.EntryWidgetClassPath);
			return Result;
		}

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		TileViewWidget->SetFlags(RF_Transactional);
		TileViewWidget->Modify();

		TileViewWidget->SetEntryWidth(Result.EntryWidth);
		TileViewWidget->SetEntryHeight(Result.EntryHeight);
		if (!SetClassPropertyValue(
				TileViewWidget->GetClass(),
				TileViewWidget,
				TEXT("EntryWidgetClass"),
				EntryWidgetClass,
				ErrorMessage) ||
			!SetEnumPropertyValue(
				TileViewWidget->GetClass(),
				TileViewWidget,
				TEXT("Orientation"),
				static_cast<int64>(OrientationValue),
				ErrorMessage))
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Configured TileView but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Configured TileView but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Configured TileView %s with entry class %s (%s %.2fx%.2f)."),
			*Result.WidgetName,
			*Result.EntryWidgetClassPath,
			*Result.Orientation,
			Result.EntryWidth,
			Result.EntryHeight);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildReorderWidgetChildObject(
		const FString& AssetPath,
		const FString& WidgetName,
		const int32 DesiredIndex,
		const bool bSaveAsset) const
	{
		const FReorderWidgetChildResult ReorderResult =
			ReorderWidgetChild(AssetPath, WidgetName, DesiredIndex, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("reordered"), ReorderResult.bReordered);
		ResultObject->SetBoolField(TEXT("saved"), ReorderResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), ReorderResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), ReorderResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), ReorderResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), ReorderResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), ReorderResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), ReorderResult.AssetName);
		ResultObject->SetStringField(TEXT("widgetName"), ReorderResult.WidgetName);
		ResultObject->SetStringField(TEXT("parentWidgetName"), ReorderResult.ParentWidgetName);
		ResultObject->SetNumberField(TEXT("requestedIndex"), ReorderResult.RequestedIndex);
		ResultObject->SetNumberField(TEXT("finalIndex"), ReorderResult.FinalIndex);
		return ResultObject;
	}

	FReorderWidgetChildResult FOctoMCPModule::ReorderWidgetChild(
		const FString& InAssetPath,
		const FString& InWidgetName,
		const int32 DesiredIndex,
		const bool bSaveAsset) const
	{
		FReorderWidgetChildResult Result;
		Result.RequestedIndex = DesiredIndex;

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
		Result.WidgetName = InWidgetName.TrimStartAndEnd();

		if (Result.WidgetName.IsEmpty())
		{
			Result.Message = TEXT("widgetName must not be empty.");
			return Result;
		}

		if (DesiredIndex < 0)
		{
			Result.Message = TEXT("desiredIndex must be zero or greater.");
			return Result;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		UWidget* const TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.WidgetName));
		if (TargetWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not find widget named %s in %s."),
				*Result.WidgetName,
				*AssetObjectPath);
			return Result;
		}

		int32 ExistingIndex = INDEX_NONE;
		UPanelWidget* const ParentWidget = UWidgetTree::FindWidgetParent(TargetWidget, ExistingIndex);
		if (ParentWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Widget %s does not have a panel parent and cannot be reordered."),
				*Result.WidgetName);
			return Result;
		}

		Result.ParentWidgetName = ParentWidget->GetName();

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		ParentWidget->SetFlags(RF_Transactional);
		ParentWidget->Modify();
		TargetWidget->SetFlags(RF_Transactional);
		TargetWidget->Modify();

		if (EnsurePanelChildAt(ParentWidget, TargetWidget, DesiredIndex) == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Failed to reorder widget %s under parent %s."),
				*Result.WidgetName,
				*Result.ParentWidgetName);
			return Result;
		}

		Result.FinalIndex = ParentWidget->GetChildIndex(TargetWidget);
		Result.bReordered = Result.FinalIndex != INDEX_NONE;
		if (!Result.bReordered)
		{
			Result.Message = FString::Printf(
				TEXT("Failed to resolve the final position of widget %s after reordering."),
				*Result.WidgetName);
			return Result;
		}

		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Reordered widget child but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Reordered widget child but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Reordered widget %s under %s to index %d."),
			*Result.WidgetName,
			*Result.ParentWidgetName,
			Result.FinalIndex);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildRemoveWidgetObject(
		const FString& AssetPath,
		const FString& WidgetName,
		const bool bSaveAsset) const
	{
		const FRemoveWidgetResult RemoveResult = RemoveWidgetFromBlueprint(AssetPath, WidgetName, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("removed"), RemoveResult.bRemoved);
		ResultObject->SetBoolField(TEXT("saved"), RemoveResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), RemoveResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), RemoveResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), RemoveResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), RemoveResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), RemoveResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), RemoveResult.AssetName);
		ResultObject->SetStringField(TEXT("widgetName"), RemoveResult.WidgetName);
		ResultObject->SetStringField(TEXT("parentWidgetName"), RemoveResult.ParentWidgetName);
		return ResultObject;
	}

	FRemoveWidgetResult FOctoMCPModule::RemoveWidgetFromBlueprint(
		const FString& InAssetPath,
		const FString& InWidgetName,
		const bool bSaveAsset) const
	{
		FRemoveWidgetResult Result;

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
		Result.WidgetName = InWidgetName.TrimStartAndEnd();

		if (Result.WidgetName.IsEmpty())
		{
			Result.Message = TEXT("widgetName must not be empty.");
			return Result;
		}

		UWidgetBlueprint* const WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetObjectPath);
		if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Widget Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		UWidget* const TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.WidgetName));
		if (TargetWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Could not find widget named %s in %s."),
				*Result.WidgetName,
				*AssetObjectPath);
			return Result;
		}

		int32 ExistingIndex = INDEX_NONE;
		if (UPanelWidget* const ParentWidget = UWidgetTree::FindWidgetParent(TargetWidget, ExistingIndex))
		{
			Result.ParentWidgetName = ParentWidget->GetName();
		}

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		TargetWidget->SetFlags(RF_Transactional);
		TargetWidget->Modify();

		TSet<UWidget*> WidgetsToDelete;
		WidgetsToDelete.Add(TargetWidget);
		FWidgetBlueprintEditorUtils::DeleteWidgets(
			WidgetBlueprint,
			WidgetsToDelete,
			FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);

		Result.bRemoved = WidgetBlueprint->WidgetTree->FindWidget(FName(*Result.WidgetName)) == nullptr;
		if (!Result.bRemoved)
		{
			Result.Message = FString::Printf(
				TEXT("Failed to remove widget %s from %s."),
				*Result.WidgetName,
				*AssetObjectPath);
			return Result;
		}

		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Removed widget but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Removed widget but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Removed widget %s from %s."),
			*Result.WidgetName,
			*AssetObjectPath);
		return Result;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildSetBlueprintClassPropertyObject(
		const FString& AssetPath,
		const FString& PropertyName,
		const FString& ValueClassPath,
		const bool bSaveAsset) const
	{
		const FSetBlueprintClassPropertyResult SetResult =
			SetBlueprintClassPropertyValue(AssetPath, PropertyName, ValueClassPath, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("saved"), SetResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), SetResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), SetResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), SetResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), SetResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), SetResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), SetResult.AssetName);
		ResultObject->SetStringField(TEXT("propertyName"), SetResult.PropertyName);
		ResultObject->SetStringField(TEXT("valueClassPath"), SetResult.ValueClassPath);
		ResultObject->SetStringField(TEXT("valueClassName"), SetResult.ValueClassName);
		return ResultObject;
	}

	FSetBlueprintClassPropertyResult FOctoMCPModule::SetBlueprintClassPropertyValue(
		const FString& InAssetPath,
		const FString& InPropertyName,
		const FString& InValueClassPath,
		const bool bSaveAsset) const
	{
		FSetBlueprintClassPropertyResult Result;

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
		Result.PropertyName = InPropertyName.TrimStartAndEnd();

		if (Result.PropertyName.IsEmpty())
		{
			Result.Message = TEXT("propertyName must not be empty.");
			return Result;
		}

		UBlueprint* const BlueprintAsset = LoadObject<UBlueprint>(nullptr, *AssetObjectPath);
		if (BlueprintAsset == nullptr)
		{
			Result.Message = FString::Printf(TEXT("Could not load Blueprint asset: %s"), *AssetObjectPath);
			return Result;
		}

		FKismetEditorUtilities::CompileBlueprint(BlueprintAsset);
		if (BlueprintAsset->GeneratedClass == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Blueprint asset does not have a generated class after compile: %s"),
				*AssetObjectPath);
			return Result;
		}

		FClassProperty* const ClassProperty =
			CastField<FClassProperty>(BlueprintAsset->GeneratedClass->FindPropertyByName(*Result.PropertyName));
		if (ClassProperty == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Blueprint class property %s was not found on %s."),
				*Result.PropertyName,
				*BlueprintAsset->GeneratedClass->GetPathName());
			return Result;
		}

		UClass* const ValueClass = ResolveClassReference(InValueClassPath, ClassProperty->MetaClass, Result.ValueClassPath, ErrorMessage);
		if (ValueClass == nullptr)
		{
			Result.Message = ErrorMessage;
			return Result;
		}

		Result.ValueClassName = ValueClass->GetName();

		UObject* const BlueprintDefaultObject = BlueprintAsset->GeneratedClass->GetDefaultObject();
		if (BlueprintDefaultObject == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Blueprint generated class does not have a default object: %s"),
				*BlueprintAsset->GeneratedClass->GetPathName());
			return Result;
		}

		BlueprintAsset->Modify();
		BlueprintDefaultObject->SetFlags(RF_Transactional);
		BlueprintDefaultObject->Modify();
		ClassProperty->SetObjectPropertyValue_InContainer(BlueprintDefaultObject, ValueClass);

		BlueprintAsset->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsModified(BlueprintAsset);
		FKismetEditorUtilities::CompileBlueprint(BlueprintAsset);

		if (bSaveAsset)
		{
			UEditorAssetSubsystem* const EditorAssetSubsystem =
				GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
			if (EditorAssetSubsystem == nullptr)
			{
				Result.Message = FString::Printf(
					TEXT("Updated Blueprint class property but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(BlueprintAsset, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Updated Blueprint class property but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Set Blueprint class property %s on %s to %s."),
			*Result.PropertyName,
			*AssetObjectPath,
			*Result.ValueClassPath);
		return Result;
	}
