#include "UEMCPMaterialTools.h"

#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString ToMaterialAssetObjectPath(const FString& InPath)
{
	if (InPath.Contains(TEXT(".")))
	{
		return InPath;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(InPath);
	if (AssetName.IsEmpty())
	{
		return InPath;
	}

	return FString::Printf(TEXT("%s.%s"), *InPath, *AssetName);
}

FString ToAssetPackagePath(const FString& InPath)
{
	if (!InPath.Contains(TEXT(".")))
	{
		return InPath;
	}

	const int32 DotIndex = InPath.Find(TEXT("."));
	return InPath.Left(DotIndex);
}

bool IsValidGameAssetPath(const FString& InPath)
{
	const FString PackagePath = ToAssetPackagePath(InPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
	const FString FolderPath = FPackageName::GetLongPackagePath(PackagePath);
	return !AssetName.IsEmpty() && !FolderPath.IsEmpty() && PackagePath.StartsWith(TEXT("/Game"));
}

bool ParsePayloadObject(const FString& PayloadJson, TSharedPtr<FJsonObject>& OutRootObject, FMCPInvokeResponse& Response)
{
	if (PayloadJson.IsEmpty())
	{
		OutRootObject = MakeShared<FJsonObject>();
		return true;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
	if (!FJsonSerializer::Deserialize(Reader, OutRootObject) || !OutRootObject.IsValid())
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("invalid_payload"), TEXT("Failed to parse material tool payload JSON."), FString(), true);
		return false;
	}

	return true;
}

template<typename TObjectType>
TObjectType* LoadAsset(const FString& AssetPath)
{
	const FString ObjectPath = ToMaterialAssetObjectPath(AssetPath);
	return LoadObject<TObjectType>(nullptr, *ObjectPath);
}

bool TryApplyColorFieldsFromRoot(
	const TSharedPtr<FJsonObject>& RootObject,
	const TCHAR* Prefix,
	FLinearColor& InOutColor)
{
	bool bChanged = false;
	const auto ApplyChannel = [&RootObject, Prefix, &bChanged](const TCHAR* Suffix, float& InOutValue)
	{
		const FString FieldName = FString::Printf(TEXT("%s_%s"), Prefix, Suffix);
		double NumberValue = 0.0;
		if (RootObject->TryGetNumberField(FieldName, NumberValue))
		{
			InOutValue = static_cast<float>(NumberValue);
			bChanged = true;
		}
	};

	ApplyChannel(TEXT("r"), InOutColor.R);
	ApplyChannel(TEXT("g"), InOutColor.G);
	ApplyChannel(TEXT("b"), InOutColor.B);
	ApplyChannel(TEXT("a"), InOutColor.A);
	return bChanged;
}

bool TryReadColorObjectField(
	const TSharedPtr<FJsonObject>& RootObject,
	const TCHAR* FieldName,
	FLinearColor& InOutColor)
{
	const TSharedPtr<FJsonObject>* ColorObject = nullptr;
	if (!RootObject->TryGetObjectField(FieldName, ColorObject) || ColorObject == nullptr || !ColorObject->IsValid())
	{
		return false;
	}

	bool bChanged = false;
	double NumberValue = 0.0;
	if ((*ColorObject)->TryGetNumberField(TEXT("r"), NumberValue))
	{
		InOutColor.R = static_cast<float>(NumberValue);
		bChanged = true;
	}
	if ((*ColorObject)->TryGetNumberField(TEXT("g"), NumberValue))
	{
		InOutColor.G = static_cast<float>(NumberValue);
		bChanged = true;
	}
	if ((*ColorObject)->TryGetNumberField(TEXT("b"), NumberValue))
	{
		InOutColor.B = static_cast<float>(NumberValue);
		bChanged = true;
	}
	if ((*ColorObject)->TryGetNumberField(TEXT("a"), NumberValue))
	{
		InOutColor.A = static_cast<float>(NumberValue);
		bChanged = true;
	}

	return bChanged;
}

template<typename TExpressionType>
TExpressionType* AddExpression(UMaterial* Material, const int32 X, const int32 Y)
{
	TExpressionType* Expression = NewObject<TExpressionType>(Material);
	Expression->MaterialExpressionEditorX = X;
	Expression->MaterialExpressionEditorY = Y;
	Material->GetExpressionCollection().AddExpression(Expression);
	return Expression;
}

EBlendMode ParseBlendMode(const FString& BlendModeText, bool& bOutRecognized)
{
	bOutRecognized = true;
	const FString Lower = BlendModeText.ToLower();
	if (Lower.IsEmpty() || Lower == TEXT("translucent"))
	{
		return BLEND_Translucent;
	}
	if (Lower == TEXT("alphacomposite") || Lower == TEXT("alpha_composite"))
	{
		return BLEND_AlphaComposite;
	}
	if (Lower == TEXT("additive"))
	{
		return BLEND_Additive;
	}

	bOutRecognized = false;
	return BLEND_Translucent;
}

void BuildSimpleDiff(const FString& Action, const FString& TargetPath, const FString& Result, FMCPInvokeResponse& Response)
{
	TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("action"), Action);
	Entry->SetStringField(TEXT("target"), TargetPath);
	Entry->SetStringField(TEXT("result"), Result);

	TArray<TSharedPtr<FJsonValue>> Entries;
	Entries.Add(MakeShared<FJsonValueObject>(Entry));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("entries"), Entries);

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Response.DiffJson);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
}

bool CreateUiGradientGraph(
	UMaterial* Material,
	const FLinearColor& TopColorDefault,
	const FLinearColor& BottomColorDefault,
	const float TopAlphaDefault,
	const float BottomAlphaDefault,
	const EBlendMode BlendMode)
{
	if (Material == nullptr)
	{
		return false;
	}

	UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
	if (EditorOnlyData == nullptr)
	{
		return false;
	}

	Material->GetExpressionCollection().Empty();
	EditorOnlyData->EmissiveColor = FColorMaterialInput();
	EditorOnlyData->Opacity = FScalarMaterialInput();

	UMaterialExpressionTextureCoordinate* TexCoord = AddExpression<UMaterialExpressionTextureCoordinate>(Material, -960, -80);
	TexCoord->CoordinateIndex = 0;
	TexCoord->UTiling = 1.0f;
	TexCoord->VTiling = 1.0f;

	UMaterialExpressionComponentMask* MaskV = AddExpression<UMaterialExpressionComponentMask>(Material, -760, -80);
	MaskV->Input.Connect(0, TexCoord);
	MaskV->R = 0;
	MaskV->G = 1;
	MaskV->B = 0;
	MaskV->A = 0;

	UMaterialExpressionVectorParameter* TopColor = AddExpression<UMaterialExpressionVectorParameter>(Material, -760, -280);
	TopColor->ParameterName = TEXT("TopColor");
	TopColor->DefaultValue = TopColorDefault;

	UMaterialExpressionVectorParameter* BottomColor = AddExpression<UMaterialExpressionVectorParameter>(Material, -760, 120);
	BottomColor->ParameterName = TEXT("BottomColor");
	BottomColor->DefaultValue = BottomColorDefault;

	UMaterialExpressionScalarParameter* TopAlpha = AddExpression<UMaterialExpressionScalarParameter>(Material, -760, 380);
	TopAlpha->ParameterName = TEXT("TopAlpha");
	TopAlpha->DefaultValue = TopAlphaDefault;

	UMaterialExpressionScalarParameter* BottomAlpha = AddExpression<UMaterialExpressionScalarParameter>(Material, -760, 560);
	BottomAlpha->ParameterName = TEXT("BottomAlpha");
	BottomAlpha->DefaultValue = BottomAlphaDefault;

	UMaterialExpressionLinearInterpolate* ColorLerp = AddExpression<UMaterialExpressionLinearInterpolate>(Material, -460, -120);
	ColorLerp->A.Connect(0, TopColor);
	ColorLerp->B.Connect(0, BottomColor);
	ColorLerp->Alpha.Connect(0, MaskV);

	UMaterialExpressionLinearInterpolate* AlphaLerp = AddExpression<UMaterialExpressionLinearInterpolate>(Material, -460, 420);
	AlphaLerp->A.Connect(0, TopAlpha);
	AlphaLerp->B.Connect(0, BottomAlpha);
	AlphaLerp->Alpha.Connect(0, MaskV);

	EditorOnlyData->EmissiveColor.Connect(0, ColorLerp);
	EditorOnlyData->Opacity.Connect(0, AlphaLerp);

	Material->MaterialDomain = MD_UI;
	Material->BlendMode = BlendMode;
	Material->SetShadingModel(MSM_Unlit);
	Material->bUseMaterialAttributes = false;

	return true;
}
}

bool FUEMCPMaterialTools::ExecuteCreateUiGradient(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
{
	TSharedPtr<FJsonObject> RootObject;
	if (!ParsePayloadObject(Request.PayloadJson, RootObject, Response))
	{
		return false;
	}

	FString TargetMaterialPath;
	RootObject->TryGetStringField(TEXT("target_material_path"), TargetMaterialPath);
	if (TargetMaterialPath.IsEmpty())
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("missing_target_material_path"), TEXT("target_material_path is required."), FString(), true);
		return false;
	}

	if (!IsValidGameAssetPath(TargetMaterialPath))
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("invalid_target_material_path"), TEXT("target_material_path must be a valid /Game path."), TargetMaterialPath, true);
		return false;
	}

	FLinearColor TopColor(1.0f, 1.0f, 1.0f, 1.0f);
	FLinearColor BottomColor(1.0f, 1.0f, 1.0f, 0.0f);
	TryApplyColorFieldsFromRoot(RootObject, TEXT("top_color"), TopColor);
	TryApplyColorFieldsFromRoot(RootObject, TEXT("bottom_color"), BottomColor);
	TryReadColorObjectField(RootObject, TEXT("top_color"), TopColor);
	TryReadColorObjectField(RootObject, TEXT("bottom_color"), BottomColor);

	double NumberValue = 0.0;
	float TopAlpha = 1.0f;
	if (RootObject->TryGetNumberField(TEXT("top_alpha"), NumberValue))
	{
		TopAlpha = static_cast<float>(NumberValue);
	}

	float BottomAlpha = 0.0f;
	if (RootObject->TryGetNumberField(TEXT("bottom_alpha"), NumberValue))
	{
		BottomAlpha = static_cast<float>(NumberValue);
	}

	FString BlendModeText;
	RootObject->TryGetStringField(TEXT("blend_mode"), BlendModeText);
	bool bRecognizedBlendMode = false;
	const EBlendMode BlendMode = ParseBlendMode(BlendModeText, bRecognizedBlendMode);
	if (!bRecognizedBlendMode)
	{
		Response.AddWarning(TEXT("unknown_blend_mode"), TEXT("Unrecognized blend_mode value. Using translucent."), BlendModeText);
	}

	bool bOverwriteGraph = true;
	RootObject->TryGetBoolField(TEXT("overwrite_graph"), bOverwriteGraph);

	UMaterial* Material = LoadAsset<UMaterial>(TargetMaterialPath);
	if (Request.bDryRun)
	{
		Response.AppliedCount += 1;
		const FString Result = (Material != nullptr && bOverwriteGraph) ? TEXT("would_rebuild") : ((Material != nullptr) ? TEXT("would_skip_existing") : TEXT("would_create"));
		BuildSimpleDiff(TEXT("material.create_ui_gradient"), TargetMaterialPath, Result, Response);
		return true;
	}

	if (Material != nullptr && !bOverwriteGraph)
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("material_exists_skipped"), TEXT("Material already exists and overwrite_graph is false."), TargetMaterialPath);
		BuildSimpleDiff(TEXT("material.create_ui_gradient"), TargetMaterialPath, TEXT("skipped_existing"), Response);
		return true;
	}

	if (Material == nullptr)
	{
		const FString PackagePath = ToAssetPackagePath(TargetMaterialPath);
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
		const FString FolderPath = FPackageName::GetLongPackagePath(PackagePath);

		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		UObject* CreatedAsset = AssetTools.CreateAsset(AssetName, FolderPath, UMaterial::StaticClass(), Factory);
		Material = Cast<UMaterial>(CreatedAsset);
		if (Material == nullptr)
		{
			Response.Status = EMCPInvokeStatus::ExecutionError;
			Response.AddError(TEXT("create_material_failed"), TEXT("Failed to create material asset."), TargetMaterialPath, true);
			return false;
		}
	}

	Material->Modify();
	const bool bGraphBuilt = CreateUiGradientGraph(Material, TopColor, BottomColor, TopAlpha, BottomAlpha, BlendMode);
	if (!bGraphBuilt)
	{
		Response.Status = EMCPInvokeStatus::ExecutionError;
		Response.AddError(TEXT("build_material_graph_failed"), TEXT("Failed to build UI gradient material graph."), TargetMaterialPath, true);
		return false;
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();

	Response.AppliedCount += 1;
	BuildSimpleDiff(TEXT("material.create_ui_gradient"), TargetMaterialPath, TEXT("applied"), Response);
	return true;
}

bool FUEMCPMaterialTools::ExecuteCreateMaterialInstance(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
{
	TSharedPtr<FJsonObject> RootObject;
	if (!ParsePayloadObject(Request.PayloadJson, RootObject, Response))
	{
		return false;
	}

	FString TargetInstancePath;
	RootObject->TryGetStringField(TEXT("target_instance_path"), TargetInstancePath);
	if (TargetInstancePath.IsEmpty())
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("missing_target_instance_path"), TEXT("target_instance_path is required."), FString(), true);
		return false;
	}

	if (!IsValidGameAssetPath(TargetInstancePath))
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("invalid_target_instance_path"), TEXT("target_instance_path must be a valid /Game path."), TargetInstancePath, true);
		return false;
	}

	FString ParentMaterialPath;
	RootObject->TryGetStringField(TEXT("parent_material_path"), ParentMaterialPath);
	if (ParentMaterialPath.IsEmpty())
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("missing_parent_material_path"), TEXT("parent_material_path is required."), FString(), true);
		return false;
	}

	if (!IsValidGameAssetPath(ParentMaterialPath))
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("invalid_parent_material_path"), TEXT("parent_material_path must be a valid /Game path."), ParentMaterialPath, true);
		return false;
	}

	UMaterialInterface* ParentMaterial = LoadAsset<UMaterialInterface>(ParentMaterialPath);
	if (ParentMaterial == nullptr)
	{
		Response.Status = EMCPInvokeStatus::ExecutionError;
		Response.AddError(TEXT("parent_material_not_found"), TEXT("Failed to load parent material."), ParentMaterialPath, true);
		return false;
	}

	UMaterialInstanceConstant* ExistingInstance = LoadAsset<UMaterialInstanceConstant>(TargetInstancePath);
	if (Request.bDryRun)
	{
		Response.AppliedCount += 1;
		const FString Result = (ExistingInstance != nullptr) ? TEXT("would_update_parent") : TEXT("would_create");
		BuildSimpleDiff(TEXT("material_instance.create"), TargetInstancePath, Result, Response);
		return true;
	}

	if (ExistingInstance != nullptr)
	{
		ExistingInstance->Modify();
		ExistingInstance->SetParentEditorOnly(ParentMaterial, false);
		ExistingInstance->PostEditChange();
		ExistingInstance->MarkPackageDirty();
		Response.AppliedCount += 1;
		BuildSimpleDiff(TEXT("material_instance.create"), TargetInstancePath, TEXT("parent_updated"), Response);
		return true;
	}

	const FString PackagePath = ToAssetPackagePath(TargetInstancePath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
	const FString FolderPath = FPackageName::GetLongPackagePath(PackagePath);

	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = ParentMaterial;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	UObject* CreatedAsset = AssetTools.CreateAsset(AssetName, FolderPath, UMaterialInstanceConstant::StaticClass(), Factory);
	UMaterialInstanceConstant* CreatedInstance = Cast<UMaterialInstanceConstant>(CreatedAsset);
	if (CreatedInstance == nullptr)
	{
		Response.Status = EMCPInvokeStatus::ExecutionError;
		Response.AddError(TEXT("create_instance_failed"), TEXT("Failed to create material instance asset."), TargetInstancePath, true);
		return false;
	}

	CreatedInstance->PostEditChange();
	CreatedInstance->MarkPackageDirty();
	Response.AppliedCount += 1;
	BuildSimpleDiff(TEXT("material_instance.create"), TargetInstancePath, TEXT("applied"), Response);
	return true;
}

bool FUEMCPMaterialTools::ExecuteSetMaterialInstanceParams(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
{
	TSharedPtr<FJsonObject> RootObject;
	if (!ParsePayloadObject(Request.PayloadJson, RootObject, Response))
	{
		return false;
	}

	FString TargetInstancePath;
	RootObject->TryGetStringField(TEXT("target_instance_path"), TargetInstancePath);
	if (TargetInstancePath.IsEmpty())
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("missing_target_instance_path"), TEXT("target_instance_path is required."), FString(), true);
		return false;
	}

	UMaterialInstanceConstant* Instance = LoadAsset<UMaterialInstanceConstant>(TargetInstancePath);
	if (Instance == nullptr)
	{
		Response.Status = EMCPInvokeStatus::ExecutionError;
		Response.AddError(TEXT("instance_not_found"), TEXT("Failed to load target material instance."), TargetInstancePath, true);
		return false;
	}

	const TSharedPtr<FJsonObject>* ScalarParamsObject = nullptr;
	const TSharedPtr<FJsonObject>* VectorParamsObject = nullptr;
	RootObject->TryGetObjectField(TEXT("scalar_params"), ScalarParamsObject);
	RootObject->TryGetObjectField(TEXT("vector_params"), VectorParamsObject);

	if ((ScalarParamsObject == nullptr || !(*ScalarParamsObject).IsValid()) &&
		(VectorParamsObject == nullptr || !(*VectorParamsObject).IsValid()))
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("missing_params"), TEXT("scalar_params or vector_params is required."), FString(), true);
		return false;
	}

	if (Request.bDryRun)
	{
		int32 ScalarCount = 0;
		int32 VectorCount = 0;
		if (ScalarParamsObject != nullptr && (*ScalarParamsObject).IsValid())
		{
			ScalarCount = (*ScalarParamsObject)->Values.Num();
		}
		if (VectorParamsObject != nullptr && (*VectorParamsObject).IsValid())
		{
			VectorCount = (*VectorParamsObject)->Values.Num();
		}

		Response.AppliedCount = ScalarCount + VectorCount;
		BuildSimpleDiff(TEXT("material_instance.set_params"), TargetInstancePath, TEXT("would_apply"), Response);
		return true;
	}

	Instance->Modify();

	if (ScalarParamsObject != nullptr && (*ScalarParamsObject).IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ScalarParamsObject)->Values)
		{
			if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Number)
			{
				Response.SkippedCount += 1;
				Response.AddWarning(TEXT("invalid_scalar_param_value"), TEXT("Scalar parameter value must be a number."), Pair.Key);
				continue;
			}

			const float ScalarValue = static_cast<float>(Pair.Value->AsNumber());
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(FName(*Pair.Key)), ScalarValue);
			Response.AppliedCount += 1;
		}
	}

	if (VectorParamsObject != nullptr && (*VectorParamsObject).IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*VectorParamsObject)->Values)
		{
			if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Object)
			{
				Response.SkippedCount += 1;
				Response.AddWarning(TEXT("invalid_vector_param_value"), TEXT("Vector parameter value must be an object with r/g/b/a."), Pair.Key);
				continue;
			}

			const TSharedPtr<FJsonObject> ValueObject = Pair.Value->AsObject();
			if (!ValueObject.IsValid())
			{
				Response.SkippedCount += 1;
				Response.AddWarning(TEXT("invalid_vector_param_value"), TEXT("Vector parameter value must be an object with r/g/b/a."), Pair.Key);
				continue;
			}

			FLinearColor Value(0.0f, 0.0f, 0.0f, 1.0f);
			double NumberValue = 0.0;
			if (ValueObject->TryGetNumberField(TEXT("r"), NumberValue))
			{
				Value.R = static_cast<float>(NumberValue);
			}
			if (ValueObject->TryGetNumberField(TEXT("g"), NumberValue))
			{
				Value.G = static_cast<float>(NumberValue);
			}
			if (ValueObject->TryGetNumberField(TEXT("b"), NumberValue))
			{
				Value.B = static_cast<float>(NumberValue);
			}
			if (ValueObject->TryGetNumberField(TEXT("a"), NumberValue))
			{
				Value.A = static_cast<float>(NumberValue);
			}

			Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(FName(*Pair.Key)), Value);
			Response.AppliedCount += 1;
		}
	}

	Instance->PostEditChange();
	Instance->MarkPackageDirty();
	BuildSimpleDiff(TEXT("material_instance.set_params"), TargetInstancePath, TEXT("applied"), Response);
	return true;
}
