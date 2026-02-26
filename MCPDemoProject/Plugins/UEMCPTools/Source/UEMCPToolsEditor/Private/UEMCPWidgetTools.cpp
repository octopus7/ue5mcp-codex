#include "UEMCPWidgetTools.h"

#include "AssetToolsModule.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Layout/Visibility.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/SlateBrush.h"
#include "UEMCPLog.h"
#include "UEMCPWidgetPatchTypes.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

namespace
{
const TCHAR* ManagedNamePrefix = TEXT("MCP_");
const TCHAR* EventBindingPrefix = TEXT("MCP_EVENT_");

FString NormalizeKey(const FString& InValue)
{
	FString Out = InValue.TrimStartAndEnd();
	for (int32 Index = 0; Index < Out.Len(); ++Index)
	{
		if (!FChar::IsAlnum(Out[Index]) && Out[Index] != TEXT('_') && Out[Index] != TEXT('.'))
		{
			Out[Index] = TEXT('_');
		}
	}
	return Out;
}

FString ToWidgetObjectPath(const FString& InPath)
{
	if (InPath.Contains(TEXT(".")))
	{
		return InPath;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(InPath);
	return FString::Printf(TEXT("%s.%s"), *InPath, *AssetName);
}

FString ToWidgetPackagePath(const FString& InPath)
{
	if (!InPath.Contains(TEXT(".")))
	{
		return InPath;
	}
	const int32 DotIndex = InPath.Find(TEXT("."));
	return InPath.Left(DotIndex);
}

FString ReadStringField(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
	FString Value;
	JsonObject->TryGetStringField(FieldName, Value);
	return Value;
}

ESlateVisibility ParseVisibilityOrDefault(const FString& InValue, ESlateVisibility DefaultValue, bool& bRecognized)
{
	const FString Lower = InValue.ToLower();
	bRecognized = true;
	if (Lower == TEXT("visible"))
	{
		return ESlateVisibility::Visible;
	}
	if (Lower == TEXT("collapsed"))
	{
		return ESlateVisibility::Collapsed;
	}
	if (Lower == TEXT("hidden"))
	{
		return ESlateVisibility::Hidden;
	}
	if (Lower == TEXT("hit_test_invisible"))
	{
		return ESlateVisibility::HitTestInvisible;
	}
	if (Lower == TEXT("self_hit_test_invisible"))
	{
		return ESlateVisibility::SelfHitTestInvisible;
	}

	bRecognized = false;
	return DefaultValue;
}

bool ParseHorizontalAlignment(const FString& InValue, EHorizontalAlignment& OutValue)
{
	const FString Lower = InValue.ToLower();
	if (Lower == TEXT("fill"))
	{
		OutValue = HAlign_Fill;
		return true;
	}
	if (Lower == TEXT("left"))
	{
		OutValue = HAlign_Left;
		return true;
	}
	if (Lower == TEXT("center"))
	{
		OutValue = HAlign_Center;
		return true;
	}
	if (Lower == TEXT("right"))
	{
		OutValue = HAlign_Right;
		return true;
	}
	return false;
}

bool ParseVerticalAlignment(const FString& InValue, EVerticalAlignment& OutValue)
{
	const FString Lower = InValue.ToLower();
	if (Lower == TEXT("fill"))
	{
		OutValue = VAlign_Fill;
		return true;
	}
	if (Lower == TEXT("top"))
	{
		OutValue = VAlign_Top;
		return true;
	}
	if (Lower == TEXT("center"))
	{
		OutValue = VAlign_Center;
		return true;
	}
	if (Lower == TEXT("bottom"))
	{
		OutValue = VAlign_Bottom;
		return true;
	}
	return false;
}

bool ParseSlateSizeRule(const FString& InValue, ESlateSizeRule::Type& OutValue)
{
	const FString Lower = InValue.ToLower();
	if (Lower == TEXT("auto") || Lower == TEXT("automatic"))
	{
		OutValue = ESlateSizeRule::Automatic;
		return true;
	}
	if (Lower == TEXT("fill"))
	{
		OutValue = ESlateSizeRule::Fill;
		return true;
	}
	return false;
}

bool ParseTextJustification(const FString& InValue, ETextJustify::Type& OutValue)
{
	const FString Lower = InValue.ToLower();
	if (Lower == TEXT("left"))
	{
		OutValue = ETextJustify::Left;
		return true;
	}
	if (Lower == TEXT("center") || Lower == TEXT("centre"))
	{
		OutValue = ETextJustify::Center;
		return true;
	}
	if (Lower == TEXT("right"))
	{
		OutValue = ETextJustify::Right;
		return true;
	}
	return false;
}

bool ParseBrushDrawType(const FString& InValue, ESlateBrushDrawType::Type& OutValue)
{
	const FString Lower = InValue.ToLower();
	if (Lower == TEXT("rounded_box") || Lower == TEXT("roundedbox"))
	{
		OutValue = ESlateBrushDrawType::RoundedBox;
		return true;
	}
	if (Lower == TEXT("box"))
	{
		OutValue = ESlateBrushDrawType::Box;
		return true;
	}
	if (Lower == TEXT("border"))
	{
		OutValue = ESlateBrushDrawType::Border;
		return true;
	}
	if (Lower == TEXT("image"))
	{
		OutValue = ESlateBrushDrawType::Image;
		return true;
	}
	if (Lower == TEXT("none") || Lower == TEXT("nodrawtype"))
	{
		OutValue = ESlateBrushDrawType::NoDrawType;
		return true;
	}
	return false;
}

bool HasColorProps(const TMap<FString, double>& NumberProps, const TCHAR* Prefix)
{
	return NumberProps.Contains(FString::Printf(TEXT("%s_r"), Prefix)) ||
		NumberProps.Contains(FString::Printf(TEXT("%s_g"), Prefix)) ||
		NumberProps.Contains(FString::Printf(TEXT("%s_b"), Prefix)) ||
		NumberProps.Contains(FString::Printf(TEXT("%s_a"), Prefix));
}

bool TryApplyColorProps(const TMap<FString, double>& NumberProps, const TCHAR* Prefix, FLinearColor& InOutColor)
{
	bool bChanged = false;
	const auto TrySetComponent = [&NumberProps, Prefix, &bChanged](const TCHAR* Suffix, float& OutComponent)
	{
		const FString Key = FString::Printf(TEXT("%s_%s"), Prefix, Suffix);
		if (const double* FoundValue = NumberProps.Find(Key))
		{
			OutComponent = static_cast<float>(*FoundValue);
			bChanged = true;
		}
	};

	TrySetComponent(TEXT("r"), InOutColor.R);
	TrySetComponent(TEXT("g"), InOutColor.G);
	TrySetComponent(TEXT("b"), InOutColor.B);
	TrySetComponent(TEXT("a"), InOutColor.A);
	return bChanged;
}

FString GetManagedKey(const UWidget* Widget);

bool IsManagedWidget(const UWidget* Widget)
{
	return !GetManagedKey(Widget).IsEmpty();
}

FString GetManagedKey(const UWidget* Widget)
{
	if (Widget == nullptr)
	{
		return FString();
	}

	const FString Name = Widget->GetName();
	if (Name.StartsWith(ManagedNamePrefix))
	{
		return Name.Mid(FCString::Strlen(ManagedNamePrefix));
	}

	return FString();
}

void SetManagedTags(UWidget* Widget, const FString& Key)
{
	if (Widget == nullptr)
	{
		return;
	}

	const FString NormalizedKey = NormalizeKey(Key);
	if (NormalizedKey.IsEmpty())
	{
		return;
	}

	const FString ManagedName = FString(ManagedNamePrefix) + NormalizedKey;
	if (Widget->GetName() == ManagedName)
	{
		return;
	}

	UObject* ExistingObject = FindObject<UObject>(Widget->GetOuter(), *ManagedName);
	if (ExistingObject != nullptr && ExistingObject != Widget)
	{
		return;
	}

	Widget->Rename(*ManagedName, Widget->GetOuter());
}

void RegisterWidgetGuid(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget)
{
	if (WidgetBlueprint == nullptr || Widget == nullptr)
	{
		return;
	}

	if (!WidgetBlueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
	{
		WidgetBlueprint->OnVariableAdded(Widget->GetFName());
	}
}

void UnregisterWidgetGuid(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget)
{
	if (WidgetBlueprint == nullptr || Widget == nullptr)
	{
		return;
	}

	WidgetBlueprint->OnVariableRemoved(Widget->GetFName());
}

void CollectWidgetSubtree(UWidget* RootWidget, TArray<UWidget*>& OutWidgets)
{
	if (RootWidget == nullptr)
	{
		return;
	}

	OutWidgets.Add(RootWidget);

	if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(RootWidget))
	{
		for (int32 ChildIndex = 0; ChildIndex < PanelWidget->GetChildrenCount(); ++ChildIndex)
		{
			CollectWidgetSubtree(PanelWidget->GetChildAt(ChildIndex), OutWidgets);
		}
	}
	else if (UContentWidget* ContentWidget = Cast<UContentWidget>(RootWidget))
	{
		CollectWidgetSubtree(ContentWidget->GetContent(), OutWidgets);
	}
}

void ReconcileWidgetVariableGuids(UWidgetBlueprint* WidgetBlueprint)
{
	if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
	{
		return;
	}

	TSet<FName> SeenVariableNames;
	TArray<UWidget*> AllWidgets;
	WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget == nullptr)
		{
			continue;
		}

		const FName WidgetName = Widget->GetFName();
		SeenVariableNames.Add(WidgetName);
		if (!WidgetBlueprint->WidgetVariableNameToGuidMap.Contains(WidgetName))
		{
			WidgetBlueprint->WidgetVariableNameToGuidMap.Add(WidgetName, FGuid::NewGuid());
		}
	}

	for (UWidgetAnimation* Animation : WidgetBlueprint->Animations)
	{
		if (Animation == nullptr)
		{
			continue;
		}

		const FName AnimationName = Animation->GetFName();
		SeenVariableNames.Add(AnimationName);
		if (!WidgetBlueprint->WidgetVariableNameToGuidMap.Contains(AnimationName))
		{
			WidgetBlueprint->WidgetVariableNameToGuidMap.Add(AnimationName, FGuid::NewGuid());
		}
	}

	for (auto It = WidgetBlueprint->WidgetVariableNameToGuidMap.CreateIterator(); It; ++It)
	{
		if (!SeenVariableNames.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
		else if (!It.Value().IsValid())
		{
			It.Value() = FGuid::NewGuid();
		}
	}
}

void BuildWidgetIndex(UWidgetTree* WidgetTree, TMap<FString, UWidget*>& OutByKey)
{
	OutByKey.Reset();
	if (WidgetTree == nullptr)
	{
		return;
	}

	TArray<UWidget*> AllWidgets;
	WidgetTree->GetAllWidgets(AllWidgets);
	for (UWidget* Widget : AllWidgets)
	{
		const FString Key = GetManagedKey(Widget);
		if (!Key.IsEmpty())
		{
			OutByKey.Add(Key, Widget);
		}
		else
		{
			const FString NameKey = NormalizeKey(Widget->GetName());
			if (!NameKey.IsEmpty() && !OutByKey.Contains(NameKey))
			{
				OutByKey.Add(NameKey, Widget);
			}
		}
	}
}

bool TryGetInsertIndex(const FMCPWidgetPatchOperation& Operation, int32& OutInsertIndex)
{
	const double* InsertIndexValue = Operation.NumberProps.Find(TEXT("insert_index"));
	if (InsertIndexValue == nullptr)
	{
		return false;
	}

	OutInsertIndex = FMath::FloorToInt(*InsertIndexValue);
	return true;
}

void AppendDiffEntry(TArray<TSharedPtr<FJsonValue>>& DiffEntries, const FString& Op, const FString& Key, const FString& Result, const FString& Detail)
{
	TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("op"), Op);
	Entry->SetStringField(TEXT("key"), Key);
	Entry->SetStringField(TEXT("result"), Result);
	Entry->SetStringField(TEXT("detail"), Detail);
	DiffEntries.Add(MakeShared<FJsonValueObject>(Entry));
}

void FinalizeDiffJson(const TArray<TSharedPtr<FJsonValue>>& DiffEntries, FMCPInvokeResponse& Response)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("entries"), DiffEntries);

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Response.DiffJson);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
}

bool ParsePatchRequest(const FString& PayloadJson, FMCPWidgetPatchRequest& OutRequest, FMCPInvokeResponse& Response)
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("invalid_payload"), TEXT("Failed to parse widget payload JSON."), FString(), true);
		return false;
	}

	OutRequest.SchemaVersion = ReadStringField(RootObject, TEXT("schema_version"));
	OutRequest.Mode = ReadStringField(RootObject, TEXT("mode"));
	OutRequest.TargetWidgetPath = ReadStringField(RootObject, TEXT("target_widget_path"));
	OutRequest.ParentClassPath = ReadStringField(RootObject, TEXT("parent_class_path"));

	if (OutRequest.SchemaVersion.IsEmpty())
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("missing_schema_version"), TEXT("schema_version is required."), FString(), true);
		return false;
	}

	if (!OutRequest.Mode.Equals(TEXT("patch"), ESearchCase::IgnoreCase))
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("unsupported_mode"), TEXT("Only mode=\"patch\" is supported."), FString(), true);
		return false;
	}

	if (OutRequest.TargetWidgetPath.IsEmpty())
	{
		FString TargetWidgetName = ReadStringField(RootObject, TEXT("target_widget_name"));
		TargetWidgetName = NormalizeKey(TargetWidgetName);
		if (TargetWidgetName.IsEmpty())
		{
			TargetWidgetName = TEXT("WBP_AutoGenerated");
		}
		else if (!TargetWidgetName.StartsWith(TEXT("WBP_")))
		{
			TargetWidgetName = FString(TEXT("WBP_")) + TargetWidgetName;
		}

		OutRequest.TargetWidgetPath = FString::Printf(TEXT("/Game/UI/Generated/%s"), *TargetWidgetName);
	}

	const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
	if (RootObject->TryGetArrayField(TEXT("operations"), OperationsArray) && OperationsArray != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& OperationValue : *OperationsArray)
		{
			if (!OperationValue.IsValid() || OperationValue->Type != EJson::Object)
			{
				continue;
			}

			const TSharedPtr<FJsonObject> OpObject = OperationValue->AsObject();
			FMCPWidgetPatchOperation& ParsedOp = OutRequest.Operations.AddDefaulted_GetRef();
			ParsedOp.Op = ReadStringField(OpObject, TEXT("op"));
			ParsedOp.Key = ReadStringField(OpObject, TEXT("key"));
			ParsedOp.ParentKey = ReadStringField(OpObject, TEXT("parent_key"));
			ParsedOp.NewParentKey = ReadStringField(OpObject, TEXT("new_parent_key"));
			ParsedOp.WidgetClass = ReadStringField(OpObject, TEXT("widget_class"));
			ParsedOp.WidgetName = ReadStringField(OpObject, TEXT("widget_name"));
			ParsedOp.VariableName = ReadStringField(OpObject, TEXT("variable_name"));
			ParsedOp.EventName = ReadStringField(OpObject, TEXT("event_name"));
			ParsedOp.FunctionName = ReadStringField(OpObject, TEXT("function_name"));

			double NumericFieldValue = 0.0;
			if (OpObject->TryGetNumberField(TEXT("insert_index"), NumericFieldValue))
			{
				ParsedOp.NumberProps.Add(TEXT("insert_index"), NumericFieldValue);
			}
			else if (OpObject->TryGetNumberField(TEXT("new_index"), NumericFieldValue))
			{
				ParsedOp.NumberProps.Add(TEXT("insert_index"), NumericFieldValue);
			}

			const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
			if (OpObject->TryGetObjectField(TEXT("properties"), PropertiesObject) && PropertiesObject != nullptr && PropertiesObject->IsValid())
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& PropertyPair : (*PropertiesObject)->Values)
				{
					switch (PropertyPair.Value->Type)
					{
					case EJson::String:
						ParsedOp.StringProps.Add(PropertyPair.Key, PropertyPair.Value->AsString());
						break;
					case EJson::Boolean:
						ParsedOp.BoolProps.Add(PropertyPair.Key, PropertyPair.Value->AsBool());
						break;
					case EJson::Number:
						ParsedOp.NumberProps.Add(PropertyPair.Key, PropertyPair.Value->AsNumber());
						break;
					default:
						break;
					}
				}
			}
		}
	}

	return true;
}

UClass* ResolveWidgetClass(const FString& WidgetClassPath)
{
	if (WidgetClassPath.IsEmpty())
	{
		return UCanvasPanel::StaticClass();
	}

	UClass* WidgetClass = LoadClass<UWidget>(nullptr, *WidgetClassPath);
	if (WidgetClass == nullptr)
	{
		WidgetClass = FindObject<UClass>(nullptr, *WidgetClassPath);
	}

	if (WidgetClass != nullptr && WidgetClass->IsChildOf(UWidget::StaticClass()))
	{
		return WidgetClass;
	}

	return nullptr;
}

UWidgetBlueprint* LoadExistingWidgetBlueprint(const FString& WidgetPath)
{
	const FString ObjectPath = ToWidgetObjectPath(WidgetPath);
	return LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
}

UWidgetBlueprint* CreateWidgetBlueprintAsset(const FMCPWidgetPatchRequest& PatchRequest, FMCPInvokeResponse& Response)
{
	const FString PackagePath = ToWidgetPackagePath(PatchRequest.TargetWidgetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
	const FString FolderPath = FPackageName::GetLongPackagePath(PackagePath);
	if (AssetName.IsEmpty() || FolderPath.IsEmpty() || !PackagePath.StartsWith(TEXT("/Game")))
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("invalid_target_widget_path"), TEXT("target_widget_path must be a valid /Game package path."), PatchRequest.TargetWidgetPath, true);
		return nullptr;
	}

	UClass* ParentClass = UUserWidget::StaticClass();
	if (!PatchRequest.ParentClassPath.IsEmpty())
	{
		UClass* LoadedParentClass = LoadClass<UUserWidget>(nullptr, *PatchRequest.ParentClassPath);
		if (LoadedParentClass == nullptr)
		{
			Response.Status = EMCPInvokeStatus::ValidationError;
			Response.AddError(TEXT("invalid_parent_class"), TEXT("Failed to load parent_class_path."), PatchRequest.ParentClassPath, true);
			return nullptr;
		}
		ParentClass = LoadedParentClass;
	}

	UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->ParentClass = ParentClass;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	UObject* CreatedAsset = AssetTools.CreateAsset(AssetName, FolderPath, UWidgetBlueprint::StaticClass(), Factory);
	UWidgetBlueprint* CreatedWidgetBlueprint = Cast<UWidgetBlueprint>(CreatedAsset);
	if (CreatedWidgetBlueprint == nullptr)
	{
		Response.Status = EMCPInvokeStatus::ExecutionError;
		Response.AddError(TEXT("create_failed"), TEXT("Failed to create Widget Blueprint asset."), PatchRequest.TargetWidgetPath, true);
		return nullptr;
	}

	if (CreatedWidgetBlueprint->WidgetTree != nullptr && CreatedWidgetBlueprint->WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* DefaultRoot = CreatedWidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		CreatedWidgetBlueprint->WidgetTree->RootWidget = DefaultRoot;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(CreatedWidgetBlueprint);
	CreatedWidgetBlueprint->MarkPackageDirty();
	return CreatedWidgetBlueprint;
}

bool EnsureWidgetBlueprintRoot(UWidgetBlueprint* WidgetBlueprint)
{
	if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
	{
		return false;
	}

	if (WidgetBlueprint->WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* RootCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetBlueprint->WidgetTree->RootWidget = RootCanvas;
	}
	return true;
}

bool ApplyAddWidgetOp(
	UWidgetBlueprint* WidgetBlueprint,
	const FMCPWidgetPatchOperation& Operation,
	bool bDryRun,
	TMap<FString, UWidget*>& WidgetByKey,
	FMCPInvokeResponse& Response,
	TArray<TSharedPtr<FJsonValue>>& DiffEntries)
{
	const FString Key = NormalizeKey(Operation.Key);
	if (Key.IsEmpty())
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("skip_missing_key"), TEXT("add_widget op requires key."), FString());
		AppendDiffEntry(DiffEntries, TEXT("add_widget"), TEXT(""), TEXT("skipped"), TEXT("Missing key"));
		return true;
	}

	if (WidgetByKey.Contains(Key))
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("skip_existing_key"), TEXT("Widget key already exists."), Key);
		AppendDiffEntry(DiffEntries, TEXT("add_widget"), Key, TEXT("skipped"), TEXT("Key already exists"));
		return true;
	}

	UClass* WidgetClass = ResolveWidgetClass(Operation.WidgetClass);
	if (WidgetClass == nullptr)
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("skip_invalid_widget_class"), TEXT("widget_class is invalid."), Operation.WidgetClass);
		AppendDiffEntry(DiffEntries, TEXT("add_widget"), Key, TEXT("skipped"), TEXT("Invalid widget_class"));
		return true;
	}

	UWidget* ParentWidget = nullptr;
	if (!Operation.ParentKey.IsEmpty())
	{
		if (UWidget* const* FoundParent = WidgetByKey.Find(NormalizeKey(Operation.ParentKey)))
		{
			ParentWidget = *FoundParent;
		}
		if (ParentWidget == nullptr)
		{
			Response.SkippedCount += 1;
			Response.AddWarning(TEXT("skip_parent_not_found"), TEXT("parent_key not found."), Operation.ParentKey);
			AppendDiffEntry(DiffEntries, TEXT("add_widget"), Key, TEXT("skipped"), TEXT("Parent key not found"));
			return true;
		}
	}
	else
	{
		ParentWidget = WidgetBlueprint->WidgetTree->RootWidget;
	}

	int32 RequestedInsertIndex = 0;
	const bool bHasInsertIndex = TryGetInsertIndex(Operation, RequestedInsertIndex);

	if (bDryRun)
	{
		Response.AppliedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("add_widget"), Key, TEXT("applied"), TEXT("Dry-run add"));
		return true;
	}

	WidgetBlueprint->Modify();
	WidgetBlueprint->WidgetTree->Modify();

	const FString DesiredName = FString(ManagedNamePrefix) + Key;
	if (!Operation.WidgetName.IsEmpty() && Operation.WidgetName != DesiredName)
	{
		Response.AddWarning(TEXT("add_widget_name_ignored"), TEXT("add_widget.widget_name is ignored to preserve MCP-managed identity."), Operation.WidgetName);
	}
	UWidget* NewWidget = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, *DesiredName);
	SetManagedTags(NewWidget, Key);
	RegisterWidgetGuid(WidgetBlueprint, NewWidget);

	if (ParentWidget == nullptr)
	{
		WidgetBlueprint->WidgetTree->RootWidget = NewWidget;
	}
	else
	{
		if (UContentWidget* ParentContent = Cast<UContentWidget>(ParentWidget))
		{
			if (ParentContent->GetContent() != nullptr)
			{
				Response.SkippedCount += 1;
				Response.AddWarning(TEXT("skip_content_parent_occupied"), TEXT("Parent content widget already has a child."), Operation.ParentKey);
				AppendDiffEntry(DiffEntries, TEXT("add_widget"), Key, TEXT("skipped"), TEXT("Parent content widget is occupied"));
				WidgetBlueprint->WidgetTree->RemoveWidget(NewWidget);
				return true;
			}

			ParentContent->Modify();
			ParentContent->SetContent(NewWidget);
			if (bHasInsertIndex)
			{
				Response.AddWarning(TEXT("insert_index_ignored"), TEXT("insert_index is ignored when parent is a content widget."), Key);
			}
		}
		else if (UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget))
		{
			ParentPanel->Modify();
			ParentPanel->AddChild(NewWidget);
			if (!ParentPanel->HasChild(NewWidget))
			{
				Response.SkippedCount += 1;
				Response.AddWarning(TEXT("skip_failed_to_attach_child"), TEXT("Failed to attach child to parent panel."), Operation.ParentKey);
				AppendDiffEntry(DiffEntries, TEXT("add_widget"), Key, TEXT("skipped"), TEXT("Failed to attach child to panel"));
				WidgetBlueprint->WidgetTree->RemoveWidget(NewWidget);
				return true;
			}

			if (bHasInsertIndex)
			{
				const int32 TargetIndex = FMath::Clamp(RequestedInsertIndex, 0, FMath::Max(0, ParentPanel->GetChildrenCount() - 1));
				ParentPanel->ShiftChild(TargetIndex, NewWidget);
			}
		}
		else
		{
			Response.SkippedCount += 1;
			Response.AddWarning(TEXT("skip_non_container_parent"), TEXT("Parent widget is not a supported container (panel/content widget)."), Operation.ParentKey);
			AppendDiffEntry(DiffEntries, TEXT("add_widget"), Key, TEXT("skipped"), TEXT("Parent is not container"));
			WidgetBlueprint->WidgetTree->RemoveWidget(NewWidget);
			return true;
		}
	}

	WidgetByKey.Add(Key, NewWidget);
	Response.AppliedCount += 1;
	AppendDiffEntry(DiffEntries, TEXT("add_widget"), Key, TEXT("applied"), TEXT("Widget added"));
	return true;
}

bool ApplyRemoveWidgetOp(
	UWidgetBlueprint* WidgetBlueprint,
	const FMCPWidgetPatchOperation& Operation,
	bool bDryRun,
	TMap<FString, UWidget*>& WidgetByKey,
	FMCPInvokeResponse& Response,
	TArray<TSharedPtr<FJsonValue>>& DiffEntries)
{
	const FString Key = NormalizeKey(Operation.Key);
	UWidget* Widget = nullptr;
	if (UWidget* const* FoundWidget = WidgetByKey.Find(Key))
	{
		Widget = *FoundWidget;
	}
	if (Widget == nullptr)
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("remove_widget"), Key, TEXT("skipped"), TEXT("Widget key not found"));
		return true;
	}

	if (!IsManagedWidget(Widget))
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("skip_manual_widget"), TEXT("remove_widget skipped because target is not MCP-managed."), Key);
		AppendDiffEntry(DiffEntries, TEXT("remove_widget"), Key, TEXT("skipped"), TEXT("Target not managed"));
		return true;
	}

	if (bDryRun)
	{
		Response.AppliedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("remove_widget"), Key, TEXT("applied"), TEXT("Dry-run remove"));
		return true;
	}

	WidgetBlueprint->Modify();
	WidgetBlueprint->WidgetTree->Modify();
	TArray<UWidget*> RemovedSubtree;
	CollectWidgetSubtree(Widget, RemovedSubtree);
	for (UWidget* RemovedWidget : RemovedSubtree)
	{
		UnregisterWidgetGuid(WidgetBlueprint, RemovedWidget);
	}
	if (WidgetBlueprint->WidgetTree->RootWidget == Widget)
	{
		WidgetBlueprint->WidgetTree->RootWidget = nullptr;
	}

	const bool bRemovedFromTree = WidgetBlueprint->WidgetTree->RemoveWidget(Widget);
	if (!bRemovedFromTree)
	{
		if (UPanelWidget* Parent = Widget->GetParent())
		{
			Parent->Modify();
			Parent->RemoveChild(Widget);
		}
	}

	WidgetByKey.Remove(Key);
	Response.AppliedCount += 1;
	AppendDiffEntry(DiffEntries, TEXT("remove_widget"), Key, TEXT("applied"), TEXT("Widget removed"));
	return true;
}

bool ApplyReparentWidgetOp(
	UWidgetBlueprint* WidgetBlueprint,
	const FMCPWidgetPatchOperation& Operation,
	bool bDryRun,
	TMap<FString, UWidget*>& WidgetByKey,
	FMCPInvokeResponse& Response,
	TArray<TSharedPtr<FJsonValue>>& DiffEntries)
{
	const FString Key = NormalizeKey(Operation.Key);
	const FString NewParentKey = NormalizeKey(Operation.NewParentKey);

	UWidget* Widget = nullptr;
	UWidget* NewParent = nullptr;
	if (UWidget* const* FoundWidget = WidgetByKey.Find(Key))
	{
		Widget = *FoundWidget;
	}
	if (UWidget* const* FoundParentWidget = WidgetByKey.Find(NewParentKey))
	{
		NewParent = *FoundParentWidget;
	}

	if (Widget == nullptr || NewParent == nullptr)
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("skipped"), TEXT("Widget or new parent not found"));
		return true;
	}

	if (!IsManagedWidget(Widget))
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("skip_manual_widget"), TEXT("reparent_widget skipped because target is not MCP-managed."), Key);
		AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("skipped"), TEXT("Target not managed"));
		return true;
	}

	if (Widget->GetParent() == nullptr && WidgetBlueprint->WidgetTree->RootWidget == Widget)
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("skip_root_reparent"), TEXT("reparent_widget cannot move the root widget directly."), Key);
		AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("skipped"), TEXT("Root widget reparent is not supported"));
		return true;
	}

	int32 RequestedInsertIndex = 0;
	const bool bHasInsertIndex = TryGetInsertIndex(Operation, RequestedInsertIndex);

	if (bDryRun)
	{
		Response.AppliedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("applied"), TEXT("Dry-run reparent"));
		return true;
	}

	WidgetBlueprint->Modify();
	if (UContentWidget* NewParentContent = Cast<UContentWidget>(NewParent))
	{
		if (NewParentContent->GetContent() != nullptr && NewParentContent->GetContent() != Widget)
		{
			Response.SkippedCount += 1;
			Response.AddWarning(TEXT("skip_content_parent_occupied"), TEXT("New parent content widget already has a child."), NewParentKey);
			AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("skipped"), TEXT("New parent content widget is occupied"));
			return true;
		}

		if (UPanelWidget* OldParent = Widget->GetParent())
		{
			if (OldParent != NewParentContent)
			{
				OldParent->Modify();
				OldParent->RemoveChild(Widget);
			}
		}

		NewParentContent->Modify();
		NewParentContent->SetContent(Widget);
		if (bHasInsertIndex)
		{
			Response.AddWarning(TEXT("insert_index_ignored"), TEXT("insert_index is ignored when new_parent_key targets a content widget."), Key);
		}

		Response.AppliedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("applied"), TEXT("Widget reparented"));
		return true;
	}

	UPanelWidget* NewParentPanel = Cast<UPanelWidget>(NewParent);
	if (NewParentPanel == nullptr)
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("skipped"), TEXT("New parent is not a container"));
		return true;
	}

	UPanelWidget* OldParent = Widget->GetParent();
	if (OldParent != NewParentPanel && !NewParentPanel->CanAddMoreChildren() && !NewParentPanel->HasChild(Widget))
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("skip_parent_full"), TEXT("New parent panel cannot accept additional children."), NewParentKey);
		AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("skipped"), TEXT("New parent panel is full"));
		return true;
	}

	if (OldParent != nullptr && OldParent != NewParentPanel)
	{
		OldParent->Modify();
		OldParent->RemoveChild(Widget);
	}

	NewParentPanel->Modify();
	if (!NewParentPanel->HasChild(Widget))
	{
		NewParentPanel->AddChild(Widget);
	}
	if (!NewParentPanel->HasChild(Widget))
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("skip_failed_to_attach_child"), TEXT("Failed to attach child to new parent panel."), NewParentKey);
		AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("skipped"), TEXT("Failed to attach child to panel"));
		return true;
	}

	if (bHasInsertIndex)
	{
		const int32 TargetIndex = FMath::Clamp(RequestedInsertIndex, 0, FMath::Max(0, NewParentPanel->GetChildrenCount() - 1));
		NewParentPanel->ShiftChild(TargetIndex, Widget);
	}

	Response.AppliedCount += 1;
	AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("applied"), TEXT("Widget reparented"));
	return true;
}

bool ApplyReorderWidgetOp(
	const FMCPWidgetPatchOperation& Operation,
	bool bDryRun,
	TMap<FString, UWidget*>& WidgetByKey,
	FMCPInvokeResponse& Response,
	TArray<TSharedPtr<FJsonValue>>& DiffEntries)
{
	const FString Key = NormalizeKey(Operation.Key);
	UWidget* Widget = nullptr;
	if (UWidget* const* FoundWidget = WidgetByKey.Find(Key))
	{
		Widget = *FoundWidget;
	}
	if (Widget == nullptr)
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("reorder_widget"), Key, TEXT("skipped"), TEXT("Widget key not found"));
		return true;
	}

	if (!IsManagedWidget(Widget))
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("skip_manual_widget"), TEXT("reorder_widget skipped because target is not MCP-managed."), Key);
		AppendDiffEntry(DiffEntries, TEXT("reorder_widget"), Key, TEXT("skipped"), TEXT("Target not managed"));
		return true;
	}

	int32 RequestedInsertIndex = 0;
	if (!TryGetInsertIndex(Operation, RequestedInsertIndex))
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("skip_missing_insert_index"), TEXT("reorder_widget requires properties.insert_index (or top-level insert_index/new_index)."), Key);
		AppendDiffEntry(DiffEntries, TEXT("reorder_widget"), Key, TEXT("skipped"), TEXT("Missing insert_index"));
		return true;
	}

	UPanelWidget* ParentPanel = Widget->GetParent();
	if (ParentPanel == nullptr)
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("reorder_widget"), Key, TEXT("skipped"), TEXT("Widget has no panel parent"));
		return true;
	}

	if (!Operation.ParentKey.IsEmpty())
	{
		const FString ParentKey = NormalizeKey(Operation.ParentKey);
		UWidget* ExpectedParent = nullptr;
		if (UWidget* const* FoundParent = WidgetByKey.Find(ParentKey))
		{
			ExpectedParent = *FoundParent;
		}
		if (ExpectedParent == nullptr || ExpectedParent != ParentPanel)
		{
			Response.SkippedCount += 1;
			AppendDiffEntry(DiffEntries, TEXT("reorder_widget"), Key, TEXT("skipped"), TEXT("parent_key does not match current parent"));
			return true;
		}
	}

	const int32 CurrentIndex = ParentPanel->GetChildIndex(Widget);
	if (CurrentIndex == INDEX_NONE)
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("reorder_widget"), Key, TEXT("skipped"), TEXT("Widget is not attached to parent"));
		return true;
	}

	const int32 TargetIndex = FMath::Clamp(RequestedInsertIndex, 0, FMath::Max(0, ParentPanel->GetChildrenCount() - 1));
	if (CurrentIndex == TargetIndex)
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("reorder_widget"), Key, TEXT("skipped"), TEXT("Already at requested index"));
		return true;
	}

	if (bDryRun)
	{
		Response.AppliedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("reorder_widget"), Key, TEXT("applied"), TEXT("Dry-run reorder"));
		return true;
	}

	ParentPanel->Modify();
	ParentPanel->ShiftChild(TargetIndex, Widget);
	Response.AppliedCount += 1;
	AppendDiffEntry(DiffEntries, TEXT("reorder_widget"), Key, TEXT("applied"), TEXT("Widget reordered"));
	return true;
}

bool ApplyUpdateWidgetOp(
	const FMCPWidgetPatchOperation& Operation,
	bool bDryRun,
	TMap<FString, UWidget*>& WidgetByKey,
	FMCPInvokeResponse& Response,
	TArray<TSharedPtr<FJsonValue>>& DiffEntries)
{
	const FString Key = NormalizeKey(Operation.Key);
	UWidget* Widget = nullptr;
	if (UWidget* const* FoundWidget = WidgetByKey.Find(Key))
	{
		Widget = *FoundWidget;
	}
	if (Widget == nullptr)
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("update_widget"), Key, TEXT("skipped"), TEXT("Widget key not found"));
		return true;
	}

	if (!IsManagedWidget(Widget))
	{
		Response.SkippedCount += 1;
		Response.AddWarning(TEXT("skip_manual_widget"), TEXT("update_widget skipped because target is not MCP-managed."), Key);
		AppendDiffEntry(DiffEntries, TEXT("update_widget"), Key, TEXT("skipped"), TEXT("Target not managed"));
		return true;
	}

	bool bAnyChange = false;
	const bool bHasSlotPropsRequested =
		Operation.StringProps.Contains(TEXT("slot_halign")) ||
		Operation.StringProps.Contains(TEXT("slot_valign")) ||
		Operation.StringProps.Contains(TEXT("slot_size_rule")) ||
		Operation.BoolProps.Contains(TEXT("slot_auto_size")) ||
		Operation.NumberProps.Contains(TEXT("slot_size_value")) ||
		Operation.NumberProps.Contains(TEXT("slot_padding")) ||
		Operation.NumberProps.Contains(TEXT("slot_padding_left")) ||
		Operation.NumberProps.Contains(TEXT("slot_padding_top")) ||
		Operation.NumberProps.Contains(TEXT("slot_padding_right")) ||
		Operation.NumberProps.Contains(TEXT("slot_padding_bottom"));
	bool bRecognizedSlotType = false;
	bool bAnySlotChange = false;

	if (Operation.BoolProps.Contains(TEXT("is_enabled")))
	{
		bAnyChange = true;
		if (!bDryRun)
		{
			Widget->Modify();
			Widget->SetIsEnabled(Operation.BoolProps[TEXT("is_enabled")]);
		}
	}

	if (Operation.StringProps.Contains(TEXT("visibility")))
	{
		bool bRecognized = false;
		const ESlateVisibility Parsed = ParseVisibilityOrDefault(Operation.StringProps[TEXT("visibility")], Widget->GetVisibility(), bRecognized);
		if (bRecognized)
		{
			bAnyChange = true;
			if (!bDryRun)
			{
				Widget->Modify();
				Widget->SetVisibility(Parsed);
			}
		}
		else
		{
			Response.AddWarning(TEXT("unknown_visibility"), TEXT("Unrecognized visibility value."), Operation.StringProps[TEXT("visibility")]);
		}
	}

	if (Operation.StringProps.Contains(TEXT("tool_tip_text")))
	{
		bAnyChange = true;
		if (!bDryRun)
		{
			Widget->Modify();
			Widget->SetToolTipText(FText::FromString(Operation.StringProps[TEXT("tool_tip_text")]));
		}
	}

	if (Operation.StringProps.Contains(TEXT("text")))
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			bAnyChange = true;
			if (!bDryRun)
			{
				TextBlock->Modify();
				TextBlock->SetText(FText::FromString(Operation.StringProps[TEXT("text")]));
			}
		}
		else
		{
			Response.AddWarning(TEXT("unsupported_text_property_target"), TEXT("text property is only supported for UTextBlock widgets."), Key);
		}
	}

	if (Operation.BoolProps.Contains(TEXT("auto_wrap_text")))
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			bAnyChange = true;
			if (!bDryRun)
			{
				TextBlock->Modify();
				TextBlock->SetAutoWrapText(Operation.BoolProps[TEXT("auto_wrap_text")]);
			}
		}
		else
		{
			Response.AddWarning(TEXT("unsupported_auto_wrap_property_target"), TEXT("auto_wrap_text property is only supported for UTextBlock widgets."), Key);
		}
	}

	if (Operation.NumberProps.Contains(TEXT("wrap_text_at")))
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			bAnyChange = true;
			if (!bDryRun)
			{
				TextBlock->Modify();
				TextBlock->SetWrapTextAt(Operation.NumberProps[TEXT("wrap_text_at")]);
			}
		}
		else
		{
			Response.AddWarning(TEXT("unsupported_wrap_text_at_property_target"), TEXT("wrap_text_at property is only supported for UTextBlock widgets."), Key);
		}
	}

	const bool bHasTextColorProps = HasColorProps(Operation.NumberProps, TEXT("text_color"));
	const bool bHasTextJustification = Operation.StringProps.Contains(TEXT("text_justification"));
	if (bHasTextColorProps || bHasTextJustification)
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			bool bTextStyleChanged = false;
			if (bHasTextColorProps)
			{
				FLinearColor Color = TextBlock->GetColorAndOpacity().GetSpecifiedColor();
				if (TryApplyColorProps(Operation.NumberProps, TEXT("text_color"), Color))
				{
					bTextStyleChanged = true;
					if (!bDryRun)
					{
						TextBlock->Modify();
						TextBlock->SetColorAndOpacity(FSlateColor(Color));
					}
				}
			}

			if (bHasTextJustification)
			{
				ETextJustify::Type Justification = ETextJustify::Left;
				if (ParseTextJustification(Operation.StringProps[TEXT("text_justification")], Justification))
				{
					bTextStyleChanged = true;
					if (!bDryRun)
					{
						TextBlock->Modify();
						TextBlock->SetJustification(Justification);
					}
				}
				else
				{
					Response.AddWarning(TEXT("unknown_text_justification"), TEXT("Unrecognized text_justification value."), Operation.StringProps[TEXT("text_justification")]);
				}
			}

			if (bTextStyleChanged)
			{
				bAnyChange = true;
			}
		}
		else
		{
			Response.AddWarning(TEXT("unsupported_text_style_target"), TEXT("text_color/text_justification are only supported for UTextBlock widgets."), Key);
		}
	}

	if (Operation.NumberProps.Contains(TEXT("render_opacity")))
	{
		bAnyChange = true;
		if (!bDryRun)
		{
			Widget->Modify();
			Widget->SetRenderOpacity(static_cast<float>(Operation.NumberProps[TEXT("render_opacity")]));
		}
	}

	auto TryGetNumberProp = [&Operation](const TCHAR* PropertyName, float& OutValue) -> bool
	{
		if (const double* FoundValue = Operation.NumberProps.Find(PropertyName))
		{
			OutValue = static_cast<float>(*FoundValue);
			return true;
		}
		return false;
	};

	if (UBorder* BorderWidget = Cast<UBorder>(Widget))
	{
		FMargin BorderPadding = BorderWidget->GetPadding();
		bool bBorderPaddingChanged = false;
		bool bBorderBrushChanged = false;
		float NumberValue = 0.0f;

		if (TryGetNumberProp(TEXT("padding"), NumberValue))
		{
			BorderPadding = FMargin(NumberValue);
			bBorderPaddingChanged = true;
		}
		if (TryGetNumberProp(TEXT("padding_left"), NumberValue))
		{
			BorderPadding.Left = NumberValue;
			bBorderPaddingChanged = true;
		}
		if (TryGetNumberProp(TEXT("padding_top"), NumberValue))
		{
			BorderPadding.Top = NumberValue;
			bBorderPaddingChanged = true;
		}
		if (TryGetNumberProp(TEXT("padding_right"), NumberValue))
		{
			BorderPadding.Right = NumberValue;
			bBorderPaddingChanged = true;
		}
		if (TryGetNumberProp(TEXT("padding_bottom"), NumberValue))
		{
			BorderPadding.Bottom = NumberValue;
			bBorderPaddingChanged = true;
		}

		if (bBorderPaddingChanged)
		{
			bAnyChange = true;
			if (!bDryRun)
			{
				BorderWidget->Modify();
				BorderWidget->SetPadding(BorderPadding);
			}
		}

		FSlateBrush BorderBrush = BorderWidget->Background;
		if (const FString* DrawAsValue = Operation.StringProps.Find(TEXT("border_draw_as")))
		{
			ESlateBrushDrawType::Type DrawType = BorderBrush.DrawAs;
			if (ParseBrushDrawType(*DrawAsValue, DrawType))
			{
				BorderBrush.DrawAs = DrawType;
				bBorderBrushChanged = true;
			}
			else
			{
				Response.AddWarning(TEXT("unknown_border_draw_as"), TEXT("Unrecognized border_draw_as value."), *DrawAsValue);
			}
		}

		FLinearColor BrushTint = BorderBrush.TintColor.GetSpecifiedColor();
		if (TryApplyColorProps(Operation.NumberProps, TEXT("brush_tint"), BrushTint))
		{
			BorderBrush.TintColor = FSlateColor(BrushTint);
			bBorderBrushChanged = true;
		}

		FSlateBrushOutlineSettings OutlineSettings = BorderBrush.OutlineSettings;
		FLinearColor OutlineColor = OutlineSettings.Color.GetSpecifiedColor();
		if (TryApplyColorProps(Operation.NumberProps, TEXT("brush_outline"), OutlineColor))
		{
			OutlineSettings.Color = FSlateColor(OutlineColor);
			bBorderBrushChanged = true;
		}

		if (TryGetNumberProp(TEXT("brush_corner_radius"), NumberValue))
		{
			OutlineSettings.CornerRadii = FVector4(NumberValue, NumberValue, NumberValue, NumberValue);
			OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			bBorderBrushChanged = true;
		}

		if (TryGetNumberProp(TEXT("brush_outline_width"), NumberValue))
		{
			OutlineSettings.Width = NumberValue;
			bBorderBrushChanged = true;
		}

		if (bBorderBrushChanged)
		{
			BorderBrush.OutlineSettings = OutlineSettings;
			bAnyChange = true;
			if (!bDryRun)
			{
				BorderWidget->Modify();
				BorderWidget->SetBrush(BorderBrush);
			}
		}
	}

	if (UButton* ButtonWidget = Cast<UButton>(Widget))
	{
		const bool bHasButtonColorProps =
			HasColorProps(Operation.NumberProps, TEXT("button_normal")) ||
			HasColorProps(Operation.NumberProps, TEXT("button_hovered")) ||
			HasColorProps(Operation.NumberProps, TEXT("button_pressed")) ||
			HasColorProps(Operation.NumberProps, TEXT("button_outline"));
		const bool bHasButtonShapeProps =
			Operation.StringProps.Contains(TEXT("button_draw_as")) ||
			Operation.NumberProps.Contains(TEXT("button_corner_radius")) ||
			Operation.NumberProps.Contains(TEXT("button_outline_width"));

		if (bHasButtonColorProps || bHasButtonShapeProps)
		{
			FButtonStyle ButtonStyle = ButtonWidget->GetStyle();
			FSlateBrush NormalBrush = ButtonStyle.Normal;
			FSlateBrush HoveredBrush = ButtonStyle.Hovered;
			FSlateBrush PressedBrush = ButtonStyle.Pressed;
			bool bButtonStyleChanged = false;
			float NumberValue = 0.0f;
			float CornerRadius = NormalBrush.OutlineSettings.CornerRadii.X;
			float OutlineWidth = NormalBrush.OutlineSettings.Width;

			if (TryGetNumberProp(TEXT("button_corner_radius"), NumberValue))
			{
				CornerRadius = NumberValue;
				bButtonStyleChanged = true;
			}
			if (TryGetNumberProp(TEXT("button_outline_width"), NumberValue))
			{
				OutlineWidth = NumberValue;
				bButtonStyleChanged = true;
			}

			ESlateBrushDrawType::Type DrawType = NormalBrush.DrawAs;
			if (const FString* DrawAsValue = Operation.StringProps.Find(TEXT("button_draw_as")))
			{
				if (!ParseBrushDrawType(*DrawAsValue, DrawType))
				{
					Response.AddWarning(TEXT("unknown_button_draw_as"), TEXT("Unrecognized button_draw_as value."), *DrawAsValue);
				}
				else
				{
					bButtonStyleChanged = true;
				}
			}

			FLinearColor OutlineColor = NormalBrush.OutlineSettings.Color.GetSpecifiedColor();
			if (TryApplyColorProps(Operation.NumberProps, TEXT("button_outline"), OutlineColor))
			{
				bButtonStyleChanged = true;
			}

			FLinearColor NormalTint = NormalBrush.TintColor.GetSpecifiedColor();
			if (TryApplyColorProps(Operation.NumberProps, TEXT("button_normal"), NormalTint))
			{
				NormalBrush.TintColor = FSlateColor(NormalTint);
				bButtonStyleChanged = true;
			}

			FLinearColor HoveredTint = HoveredBrush.TintColor.GetSpecifiedColor();
			if (TryApplyColorProps(Operation.NumberProps, TEXT("button_hovered"), HoveredTint))
			{
				HoveredBrush.TintColor = FSlateColor(HoveredTint);
				bButtonStyleChanged = true;
			}

			FLinearColor PressedTint = PressedBrush.TintColor.GetSpecifiedColor();
			if (TryApplyColorProps(Operation.NumberProps, TEXT("button_pressed"), PressedTint))
			{
				PressedBrush.TintColor = FSlateColor(PressedTint);
				bButtonStyleChanged = true;
			}

			const FSlateBrushOutlineSettings OutlineSettings(
				FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius),
				FSlateColor(OutlineColor),
				OutlineWidth);

			NormalBrush.DrawAs = DrawType;
			HoveredBrush.DrawAs = DrawType;
			PressedBrush.DrawAs = DrawType;
			NormalBrush.OutlineSettings = OutlineSettings;
			HoveredBrush.OutlineSettings = OutlineSettings;
			PressedBrush.OutlineSettings = OutlineSettings;

			if (bButtonStyleChanged)
			{
				ButtonStyle.Normal = NormalBrush;
				ButtonStyle.Hovered = HoveredBrush;
				ButtonStyle.Pressed = PressedBrush;
				bAnyChange = true;
				if (!bDryRun)
				{
					ButtonWidget->Modify();
					ButtonWidget->SetStyle(ButtonStyle);
				}
			}
		}
	}

	const bool bHasSliderProps =
		Operation.NumberProps.Contains(TEXT("slider_min_value")) ||
		Operation.NumberProps.Contains(TEXT("slider_max_value")) ||
		Operation.NumberProps.Contains(TEXT("slider_value")) ||
		Operation.NumberProps.Contains(TEXT("slider_step_size"));

	if (bHasSliderProps)
	{
		if (USlider* SliderWidget = Cast<USlider>(Widget))
		{
			float MinValue = SliderWidget->GetMinValue();
			float MaxValue = SliderWidget->GetMaxValue();
			float SliderValue = SliderWidget->GetValue();
			float StepSize = SliderWidget->GetStepSize();
			float NumberValue = 0.0f;
			bool bSliderChanged = false;

			if (TryGetNumberProp(TEXT("slider_min_value"), NumberValue))
			{
				MinValue = NumberValue;
				bSliderChanged = true;
			}
			if (TryGetNumberProp(TEXT("slider_max_value"), NumberValue))
			{
				MaxValue = NumberValue;
				bSliderChanged = true;
			}
			if (TryGetNumberProp(TEXT("slider_value"), NumberValue))
			{
				SliderValue = NumberValue;
				bSliderChanged = true;
			}
			if (TryGetNumberProp(TEXT("slider_step_size"), NumberValue))
			{
				StepSize = NumberValue;
				bSliderChanged = true;
			}

			if (bSliderChanged)
			{
				bAnyChange = true;
				if (!bDryRun)
				{
					SliderWidget->Modify();
					SliderWidget->SetMinValue(MinValue);
					SliderWidget->SetMaxValue(MaxValue);
					SliderWidget->SetStepSize(StepSize);
					SliderWidget->SetValue(SliderValue);
				}
			}
		}
		else
		{
			Response.AddWarning(TEXT("unsupported_slider_property_target"), TEXT("slider_* properties are only supported for USlider widgets."), Key);
		}
	}

	if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
	{
		bRecognizedSlotType = true;
		FMargin SlotPadding = VerticalBoxSlot->GetPadding();
		EHorizontalAlignment SlotHAlign = VerticalBoxSlot->GetHorizontalAlignment();
		EVerticalAlignment SlotVAlign = VerticalBoxSlot->GetVerticalAlignment();
		FSlateChildSize SlotSize = VerticalBoxSlot->GetSize();
		bool bVerticalSlotChanged = false;
		float NumberValue = 0.0f;

		if (TryGetNumberProp(TEXT("slot_padding"), NumberValue))
		{
			SlotPadding = FMargin(NumberValue);
			bVerticalSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_left"), NumberValue))
		{
			SlotPadding.Left = NumberValue;
			bVerticalSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_top"), NumberValue))
		{
			SlotPadding.Top = NumberValue;
			bVerticalSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_right"), NumberValue))
		{
			SlotPadding.Right = NumberValue;
			bVerticalSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_bottom"), NumberValue))
		{
			SlotPadding.Bottom = NumberValue;
			bVerticalSlotChanged = true;
		}

		if (const FString* SlotHAlignValue = Operation.StringProps.Find(TEXT("slot_halign")))
		{
			EHorizontalAlignment Parsed = SlotHAlign;
			if (ParseHorizontalAlignment(*SlotHAlignValue, Parsed))
			{
				SlotHAlign = Parsed;
				bVerticalSlotChanged = true;
			}
			else
			{
				Response.AddWarning(TEXT("unknown_slot_halign"), TEXT("Unrecognized slot_halign value."), *SlotHAlignValue);
			}
		}

		if (const FString* SlotVAlignValue = Operation.StringProps.Find(TEXT("slot_valign")))
		{
			EVerticalAlignment Parsed = SlotVAlign;
			if (ParseVerticalAlignment(*SlotVAlignValue, Parsed))
			{
				SlotVAlign = Parsed;
				bVerticalSlotChanged = true;
			}
			else
			{
				Response.AddWarning(TEXT("unknown_slot_valign"), TEXT("Unrecognized slot_valign value."), *SlotVAlignValue);
			}
		}

		if (const FString* SlotSizeRuleValue = Operation.StringProps.Find(TEXT("slot_size_rule")))
		{
			ESlateSizeRule::Type ParsedRule = SlotSize.SizeRule;
			if (ParseSlateSizeRule(*SlotSizeRuleValue, ParsedRule))
			{
				SlotSize.SizeRule = ParsedRule;
				bVerticalSlotChanged = true;
			}
			else
			{
				Response.AddWarning(TEXT("unknown_slot_size_rule"), TEXT("Unrecognized slot_size_rule value."), *SlotSizeRuleValue);
			}
		}

		if (TryGetNumberProp(TEXT("slot_size_value"), NumberValue))
		{
			SlotSize.Value = NumberValue;
			bVerticalSlotChanged = true;
		}

		if (bVerticalSlotChanged)
		{
			bAnySlotChange = true;
			bAnyChange = true;
			if (!bDryRun)
			{
				VerticalBoxSlot->Modify();
				VerticalBoxSlot->SetPadding(SlotPadding);
				VerticalBoxSlot->SetHorizontalAlignment(SlotHAlign);
				VerticalBoxSlot->SetVerticalAlignment(SlotVAlign);
				VerticalBoxSlot->SetSize(SlotSize);
			}
		}
	}

	if (UHorizontalBoxSlot* HorizontalBoxSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
	{
		bRecognizedSlotType = true;
		FMargin SlotPadding = HorizontalBoxSlot->GetPadding();
		EHorizontalAlignment SlotHAlign = HorizontalBoxSlot->GetHorizontalAlignment();
		EVerticalAlignment SlotVAlign = HorizontalBoxSlot->GetVerticalAlignment();
		FSlateChildSize SlotSize = HorizontalBoxSlot->GetSize();
		bool bHorizontalSlotChanged = false;
		float NumberValue = 0.0f;

		if (TryGetNumberProp(TEXT("slot_padding"), NumberValue))
		{
			SlotPadding = FMargin(NumberValue);
			bHorizontalSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_left"), NumberValue))
		{
			SlotPadding.Left = NumberValue;
			bHorizontalSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_top"), NumberValue))
		{
			SlotPadding.Top = NumberValue;
			bHorizontalSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_right"), NumberValue))
		{
			SlotPadding.Right = NumberValue;
			bHorizontalSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_bottom"), NumberValue))
		{
			SlotPadding.Bottom = NumberValue;
			bHorizontalSlotChanged = true;
		}

		if (const FString* SlotHAlignValue = Operation.StringProps.Find(TEXT("slot_halign")))
		{
			EHorizontalAlignment Parsed = SlotHAlign;
			if (ParseHorizontalAlignment(*SlotHAlignValue, Parsed))
			{
				SlotHAlign = Parsed;
				bHorizontalSlotChanged = true;
			}
			else
			{
				Response.AddWarning(TEXT("unknown_slot_halign"), TEXT("Unrecognized slot_halign value."), *SlotHAlignValue);
			}
		}

		if (const FString* SlotVAlignValue = Operation.StringProps.Find(TEXT("slot_valign")))
		{
			EVerticalAlignment Parsed = SlotVAlign;
			if (ParseVerticalAlignment(*SlotVAlignValue, Parsed))
			{
				SlotVAlign = Parsed;
				bHorizontalSlotChanged = true;
			}
			else
			{
				Response.AddWarning(TEXT("unknown_slot_valign"), TEXT("Unrecognized slot_valign value."), *SlotVAlignValue);
			}
		}

		if (const FString* SlotSizeRuleValue = Operation.StringProps.Find(TEXT("slot_size_rule")))
		{
			ESlateSizeRule::Type ParsedRule = SlotSize.SizeRule;
			if (ParseSlateSizeRule(*SlotSizeRuleValue, ParsedRule))
			{
				SlotSize.SizeRule = ParsedRule;
				bHorizontalSlotChanged = true;
			}
			else
			{
				Response.AddWarning(TEXT("unknown_slot_size_rule"), TEXT("Unrecognized slot_size_rule value."), *SlotSizeRuleValue);
			}
		}

		if (TryGetNumberProp(TEXT("slot_size_value"), NumberValue))
		{
			SlotSize.Value = NumberValue;
			bHorizontalSlotChanged = true;
		}

		if (bHorizontalSlotChanged)
		{
			bAnySlotChange = true;
			bAnyChange = true;
			if (!bDryRun)
			{
				HorizontalBoxSlot->Modify();
				HorizontalBoxSlot->SetPadding(SlotPadding);
				HorizontalBoxSlot->SetHorizontalAlignment(SlotHAlign);
				HorizontalBoxSlot->SetVerticalAlignment(SlotVAlign);
				HorizontalBoxSlot->SetSize(SlotSize);
			}
		}
	}

	if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(Widget->Slot))
	{
		bRecognizedSlotType = true;
		FMargin SlotPadding = BorderSlot->GetPadding();
		EHorizontalAlignment SlotHAlign = BorderSlot->GetHorizontalAlignment();
		EVerticalAlignment SlotVAlign = BorderSlot->GetVerticalAlignment();
		bool bBorderSlotChanged = false;
		float NumberValue = 0.0f;

		if (TryGetNumberProp(TEXT("slot_padding"), NumberValue))
		{
			SlotPadding = FMargin(NumberValue);
			bBorderSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_left"), NumberValue))
		{
			SlotPadding.Left = NumberValue;
			bBorderSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_top"), NumberValue))
		{
			SlotPadding.Top = NumberValue;
			bBorderSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_right"), NumberValue))
		{
			SlotPadding.Right = NumberValue;
			bBorderSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_padding_bottom"), NumberValue))
		{
			SlotPadding.Bottom = NumberValue;
			bBorderSlotChanged = true;
		}

		if (const FString* SlotHAlignValue = Operation.StringProps.Find(TEXT("slot_halign")))
		{
			EHorizontalAlignment Parsed = SlotHAlign;
			if (ParseHorizontalAlignment(*SlotHAlignValue, Parsed))
			{
				SlotHAlign = Parsed;
				bBorderSlotChanged = true;
			}
			else
			{
				Response.AddWarning(TEXT("unknown_slot_halign"), TEXT("Unrecognized slot_halign value."), *SlotHAlignValue);
			}
		}

		if (const FString* SlotVAlignValue = Operation.StringProps.Find(TEXT("slot_valign")))
		{
			EVerticalAlignment Parsed = SlotVAlign;
			if (ParseVerticalAlignment(*SlotVAlignValue, Parsed))
			{
				SlotVAlign = Parsed;
				bBorderSlotChanged = true;
			}
			else
			{
				Response.AddWarning(TEXT("unknown_slot_valign"), TEXT("Unrecognized slot_valign value."), *SlotVAlignValue);
			}
		}

		if (bBorderSlotChanged)
		{
			bAnySlotChange = true;
			bAnyChange = true;
			if (!bDryRun)
			{
				BorderSlot->Modify();
				BorderSlot->SetPadding(SlotPadding);
				BorderSlot->SetHorizontalAlignment(SlotHAlign);
				BorderSlot->SetVerticalAlignment(SlotVAlign);
			}
		}
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		bRecognizedSlotType = true;
		FAnchors Anchors = CanvasSlot->GetAnchors();
		FVector2D Alignment = CanvasSlot->GetAlignment();
		FVector2D Position = CanvasSlot->GetPosition();
		FVector2D Size = CanvasSlot->GetSize();
		int32 ZOrder = CanvasSlot->GetZOrder();
		bool bAutoSize = CanvasSlot->GetAutoSize();

		bool bCanvasSlotChanged = false;
		float NumberValue = 0.0f;
		if (TryGetNumberProp(TEXT("slot_anchors_min_x"), NumberValue))
		{
			Anchors.Minimum.X = NumberValue;
			bCanvasSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_anchors_min_y"), NumberValue))
		{
			Anchors.Minimum.Y = NumberValue;
			bCanvasSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_anchors_max_x"), NumberValue))
		{
			Anchors.Maximum.X = NumberValue;
			bCanvasSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_anchors_max_y"), NumberValue))
		{
			Anchors.Maximum.Y = NumberValue;
			bCanvasSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_alignment_x"), NumberValue))
		{
			Alignment.X = NumberValue;
			bCanvasSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_alignment_y"), NumberValue))
		{
			Alignment.Y = NumberValue;
			bCanvasSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_position_x"), NumberValue))
		{
			Position.X = NumberValue;
			bCanvasSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_position_y"), NumberValue))
		{
			Position.Y = NumberValue;
			bCanvasSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_size_x"), NumberValue))
		{
			Size.X = NumberValue;
			bCanvasSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_size_y"), NumberValue))
		{
			Size.Y = NumberValue;
			bCanvasSlotChanged = true;
		}
		if (TryGetNumberProp(TEXT("slot_z_order"), NumberValue))
		{
			ZOrder = FMath::RoundToInt(NumberValue);
			bCanvasSlotChanged = true;
		}
		if (Operation.BoolProps.Contains(TEXT("slot_auto_size")))
		{
			bAutoSize = Operation.BoolProps[TEXT("slot_auto_size")];
			bCanvasSlotChanged = true;
		}

		if (bCanvasSlotChanged)
		{
			bAnySlotChange = true;
			bAnyChange = true;
			if (!bDryRun)
			{
				CanvasSlot->Modify();
				CanvasSlot->SetAnchors(Anchors);
				CanvasSlot->SetAlignment(Alignment);
				CanvasSlot->SetPosition(Position);
				CanvasSlot->SetSize(Size);
				CanvasSlot->SetZOrder(ZOrder);
				CanvasSlot->SetAutoSize(bAutoSize);
			}
		}
	}

	if (bHasSlotPropsRequested && !bRecognizedSlotType)
	{
		const FString SlotClassName = Widget->Slot != nullptr ? Widget->Slot->GetClass()->GetName() : FString(TEXT("null"));
		Response.AddWarning(TEXT("slot_type_not_supported"), TEXT("slot_* properties were provided but this widget slot type is unsupported."), FString::Printf(TEXT("%s (slot=%s)"), *Key, *SlotClassName));
	}
	else if (bHasSlotPropsRequested && !bAnySlotChange)
	{
		const FString SlotClassName = Widget->Slot != nullptr ? Widget->Slot->GetClass()->GetName() : FString(TEXT("null"));
		Response.AddWarning(TEXT("slot_properties_not_applied"), TEXT("slot_* properties were provided but no slot layout change was applied."), FString::Printf(TEXT("%s (slot=%s)"), *Key, *SlotClassName));
	}

	if (!Operation.WidgetName.IsEmpty())
	{
		Response.AddWarning(TEXT("widget_rename_ignored"), TEXT("widget_name update is ignored to avoid variable GUID and object identity conflicts."), Operation.WidgetName);
	}

	if (bAnyChange)
	{
		Response.AppliedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("update_widget"), Key, TEXT("applied"), TEXT("Widget updated"));
	}
	else
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("update_widget"), Key, TEXT("skipped"), TEXT("No supported properties found"));
	}

	return true;
}

bool ApplyBindWidgetRefOp(
	UWidgetBlueprint* WidgetBlueprint,
	const FMCPWidgetPatchOperation& Operation,
	bool bDryRun,
	TMap<FString, UWidget*>& WidgetByKey,
	FMCPInvokeResponse& Response,
	TArray<TSharedPtr<FJsonValue>>& DiffEntries)
{
	const FString Key = NormalizeKey(Operation.Key);
	UWidget* Widget = nullptr;
	if (UWidget* const* FoundWidget = WidgetByKey.Find(Key))
	{
		Widget = *FoundWidget;
	}
	if (Widget == nullptr)
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("bind_widget_ref"), Key, TEXT("skipped"), TEXT("Widget key not found"));
		return true;
	}

	if (!bDryRun)
	{
		Widget->Modify();
		if (FBoolProperty* IsVariableProperty = FindFProperty<FBoolProperty>(UWidget::StaticClass(), TEXT("bIsVariable")))
		{
			IsVariableProperty->SetPropertyValue_InContainer(Widget, true);
		}

		if (!Operation.VariableName.IsEmpty() && Widget->GetFName().ToString() != Operation.VariableName)
		{
			Response.AddWarning(TEXT("bind_ref_variable_name_ignored"), TEXT("variable_name is ignored to preserve MCP-managed identity."), Operation.VariableName);
		}
	}

	Response.AppliedCount += 1;
	AppendDiffEntry(DiffEntries, TEXT("bind_widget_ref"), Key, TEXT("applied"), TEXT("Widget reference binding applied"));
	return true;
}

bool ApplyBindEventOp(
	const FMCPWidgetPatchOperation& Operation,
	bool bDryRun,
	TMap<FString, UWidget*>& WidgetByKey,
	FMCPInvokeResponse& Response,
	TArray<TSharedPtr<FJsonValue>>& DiffEntries)
{
	const FString Key = NormalizeKey(Operation.Key);
	UWidget* Widget = nullptr;
	if (UWidget* const* FoundWidget = WidgetByKey.Find(Key))
	{
		Widget = *FoundWidget;
	}
	if (Widget == nullptr)
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("bind_event"), Key, TEXT("skipped"), TEXT("Widget key not found"));
		return true;
	}

	if (Operation.EventName.IsEmpty() || Operation.FunctionName.IsEmpty())
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("bind_event"), Key, TEXT("skipped"), TEXT("event_name/function_name required"));
		return true;
	}

	const FString BindingTag = FString::Printf(TEXT("%s%s.%s"), EventBindingPrefix, *NormalizeKey(Operation.EventName), *NormalizeKey(Operation.FunctionName));
	if (!bDryRun)
	{
		Widget->Modify();
	}

	Response.AppliedCount += 1;
	Response.AddWarning(
		TEXT("event_binding_recorded"),
		TEXT("Event binding request is accepted but graph-level delegate wiring is not auto-generated in M1."),
		BindingTag);
	AppendDiffEntry(DiffEntries, TEXT("bind_event"), Key, TEXT("applied"), TEXT("Event binding metadata recorded"));
	return true;
}

bool ApplyOperationsToWidgetBlueprint(
	UWidgetBlueprint* WidgetBlueprint,
	const FMCPWidgetPatchRequest& PatchRequest,
	bool bDryRun,
	FMCPInvokeResponse& Response,
	bool bOnlyBindingOps)
{
	if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
	{
		Response.Status = EMCPInvokeStatus::ExecutionError;
		Response.AddError(TEXT("invalid_widget_blueprint"), TEXT("Widget blueprint is invalid or missing WidgetTree."), FString(), true);
		return false;
	}

	EnsureWidgetBlueprintRoot(WidgetBlueprint);

	TMap<FString, UWidget*> WidgetByKey;
	BuildWidgetIndex(WidgetBlueprint->WidgetTree, WidgetByKey);

	TArray<TSharedPtr<FJsonValue>> DiffEntries;
	for (const FMCPWidgetPatchOperation& Operation : PatchRequest.Operations)
	{
		const FString OpLower = Operation.Op.ToLower();
		if (OpLower.IsEmpty())
		{
			Response.SkippedCount += 1;
			AppendDiffEntry(DiffEntries, TEXT("unknown"), NormalizeKey(Operation.Key), TEXT("skipped"), TEXT("Missing op"));
			continue;
		}

		if (bOnlyBindingOps && OpLower != TEXT("bind_widget_ref") && OpLower != TEXT("bind_event"))
		{
			Response.SkippedCount += 1;
			AppendDiffEntry(DiffEntries, OpLower, NormalizeKey(Operation.Key), TEXT("skipped"), TEXT("Filtered by bind-only mode"));
			continue;
		}

		if (OpLower == TEXT("add_widget"))
		{
			ApplyAddWidgetOp(WidgetBlueprint, Operation, bDryRun, WidgetByKey, Response, DiffEntries);
		}
		else if (OpLower == TEXT("update_widget"))
		{
			ApplyUpdateWidgetOp(Operation, bDryRun, WidgetByKey, Response, DiffEntries);
		}
		else if (OpLower == TEXT("remove_widget"))
		{
			ApplyRemoveWidgetOp(WidgetBlueprint, Operation, bDryRun, WidgetByKey, Response, DiffEntries);
		}
		else if (OpLower == TEXT("reparent_widget"))
		{
			ApplyReparentWidgetOp(WidgetBlueprint, Operation, bDryRun, WidgetByKey, Response, DiffEntries);
		}
		else if (OpLower == TEXT("reorder_widget"))
		{
			ApplyReorderWidgetOp(Operation, bDryRun, WidgetByKey, Response, DiffEntries);
		}
		else if (OpLower == TEXT("bind_widget_ref"))
		{
			ApplyBindWidgetRefOp(WidgetBlueprint, Operation, bDryRun, WidgetByKey, Response, DiffEntries);
		}
		else if (OpLower == TEXT("bind_event"))
		{
			ApplyBindEventOp(Operation, bDryRun, WidgetByKey, Response, DiffEntries);
		}
		else
		{
			Response.SkippedCount += 1;
			Response.AddWarning(TEXT("unknown_operation"), TEXT("Unknown operation in widget patch request."), Operation.Op);
			AppendDiffEntry(DiffEntries, Operation.Op, NormalizeKey(Operation.Key), TEXT("skipped"), TEXT("Unknown operation"));
		}
	}

	FinalizeDiffJson(DiffEntries, Response);
	if (!bDryRun)
	{
		ReconcileWidgetVariableGuids(WidgetBlueprint);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
		WidgetBlueprint->MarkPackageDirty();
	}

	return true;
}

bool ParseRepairRequest(
	const FString& PayloadJson,
	FString& OutTargetWidgetPath,
	TMap<FString, FString>& OutExpectedParents,
	FMCPInvokeResponse& Response)
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("invalid_payload"), TEXT("Failed to parse widget repair payload JSON."), FString(), true);
		return false;
	}

	RootObject->TryGetStringField(TEXT("target_widget_path"), OutTargetWidgetPath);
	if (OutTargetWidgetPath.IsEmpty())
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("missing_target_widget_path"), TEXT("target_widget_path is required."), FString(), true);
		return false;
	}

	const TSharedPtr<FJsonObject>* ParentsObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("expected_parents"), ParentsObject) || ParentsObject == nullptr || !ParentsObject->IsValid())
	{
		Response.Status = EMCPInvokeStatus::ValidationError;
		Response.AddError(TEXT("missing_expected_parents"), TEXT("expected_parents object is required for widget.repair_tree."), FString(), true);
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ParentsObject)->Values)
	{
		if (Pair.Value.IsValid() && Pair.Value->Type == EJson::String)
		{
			OutExpectedParents.Add(NormalizeKey(Pair.Key), NormalizeKey(Pair.Value->AsString()));
		}
	}
	return true;
}
}

bool FUEMCPWidgetTools::ExecuteCreateOrPatch(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
{
	FMCPWidgetPatchRequest PatchRequest;
	if (!ParsePatchRequest(Request.PayloadJson, PatchRequest, Response))
	{
		return false;
	}

	UWidgetBlueprint* WidgetBlueprint = LoadExistingWidgetBlueprint(PatchRequest.TargetWidgetPath);
	if (WidgetBlueprint == nullptr)
	{
		if (Request.bDryRun)
		{
			Response.AddWarning(TEXT("dry_run_create_required"), TEXT("Widget blueprint does not exist and would be created in non-dry-run mode."), PatchRequest.TargetWidgetPath);
			TArray<TSharedPtr<FJsonValue>> DiffEntries;
			AppendDiffEntry(DiffEntries, TEXT("create_widget_blueprint"), TEXT(""), TEXT("applied"), TEXT("Dry-run create"));
			FinalizeDiffJson(DiffEntries, Response);
			return true;
		}

		WidgetBlueprint = CreateWidgetBlueprintAsset(PatchRequest, Response);
		if (WidgetBlueprint == nullptr)
		{
			return false;
		}
	}

	return ApplyOperationsToWidgetBlueprint(WidgetBlueprint, PatchRequest, Request.bDryRun, Response, false);
}

bool FUEMCPWidgetTools::ExecuteApplyOps(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
{
	FMCPWidgetPatchRequest PatchRequest;
	if (!ParsePatchRequest(Request.PayloadJson, PatchRequest, Response))
	{
		return false;
	}

	UWidgetBlueprint* WidgetBlueprint = LoadExistingWidgetBlueprint(PatchRequest.TargetWidgetPath);
	if (WidgetBlueprint == nullptr)
	{
		Response.Status = EMCPInvokeStatus::ExecutionError;
		Response.AddError(TEXT("widget_not_found"), TEXT("Target widget blueprint does not exist."), PatchRequest.TargetWidgetPath, true);
		return false;
	}

	return ApplyOperationsToWidgetBlueprint(WidgetBlueprint, PatchRequest, Request.bDryRun, Response, false);
}

bool FUEMCPWidgetTools::ExecuteBindRefsAndEvents(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
{
	FMCPWidgetPatchRequest PatchRequest;
	if (!ParsePatchRequest(Request.PayloadJson, PatchRequest, Response))
	{
		return false;
	}

	UWidgetBlueprint* WidgetBlueprint = LoadExistingWidgetBlueprint(PatchRequest.TargetWidgetPath);
	if (WidgetBlueprint == nullptr)
	{
		Response.Status = EMCPInvokeStatus::ExecutionError;
		Response.AddError(TEXT("widget_not_found"), TEXT("Target widget blueprint does not exist."), PatchRequest.TargetWidgetPath, true);
		return false;
	}

	return ApplyOperationsToWidgetBlueprint(WidgetBlueprint, PatchRequest, Request.bDryRun, Response, true);
}

bool FUEMCPWidgetTools::ExecuteRepairTree(const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
{
	FString TargetWidgetPath;
	TMap<FString, FString> ExpectedParents;
	if (!ParseRepairRequest(Request.PayloadJson, TargetWidgetPath, ExpectedParents, Response))
	{
		return false;
	}

	UWidgetBlueprint* WidgetBlueprint = LoadExistingWidgetBlueprint(TargetWidgetPath);
	if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
	{
		Response.Status = EMCPInvokeStatus::ExecutionError;
		Response.AddError(TEXT("widget_not_found"), TEXT("Target widget blueprint does not exist."), TargetWidgetPath, true);
		return false;
	}

	TMap<FString, UWidget*> WidgetByKey;
	BuildWidgetIndex(WidgetBlueprint->WidgetTree, WidgetByKey);

	TArray<TSharedPtr<FJsonValue>> DiffEntries;
	for (const TPair<FString, FString>& Pair : ExpectedParents)
	{
		const FString ChildKey = NormalizeKey(Pair.Key);
		const FString ParentKey = NormalizeKey(Pair.Value);

		UWidget* ChildWidget = nullptr;
		UWidget* TargetParentWidget = nullptr;
		if (UWidget* const* FoundChild = WidgetByKey.Find(ChildKey))
		{
			ChildWidget = *FoundChild;
		}
		if (UWidget* const* FoundParent = WidgetByKey.Find(ParentKey))
		{
			TargetParentWidget = *FoundParent;
		}

		if (ChildWidget == nullptr || TargetParentWidget == nullptr)
		{
			Response.SkippedCount += 1;
			AppendDiffEntry(DiffEntries, TEXT("repair_reparent"), ChildKey, TEXT("skipped"), TEXT("Child or parent key missing"));
			continue;
		}

		if (!IsManagedWidget(ChildWidget))
		{
			Response.SkippedCount += 1;
			AppendDiffEntry(DiffEntries, TEXT("repair_reparent"), ChildKey, TEXT("skipped"), TEXT("Target is not managed"));
			continue;
		}

		UPanelWidget* TargetParentPanel = Cast<UPanelWidget>(TargetParentWidget);
		if (TargetParentPanel == nullptr)
		{
			Response.SkippedCount += 1;
			AppendDiffEntry(DiffEntries, TEXT("repair_reparent"), ChildKey, TEXT("skipped"), TEXT("Parent is not panel"));
			continue;
		}

		if (ChildWidget->GetParent() == TargetParentPanel)
		{
			Response.SkippedCount += 1;
			AppendDiffEntry(DiffEntries, TEXT("repair_reparent"), ChildKey, TEXT("skipped"), TEXT("Already attached to expected parent"));
			continue;
		}

		if (!Request.bDryRun)
		{
			WidgetBlueprint->Modify();
			if (UPanelWidget* ExistingParent = ChildWidget->GetParent())
			{
				ExistingParent->Modify();
				ExistingParent->RemoveChild(ChildWidget);
			}
			TargetParentPanel->Modify();
			TargetParentPanel->AddChild(ChildWidget);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
			WidgetBlueprint->MarkPackageDirty();
		}

		Response.AppliedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("repair_reparent"), ChildKey, TEXT("applied"), TEXT("Repaired parent relationship"));
	}

	FinalizeDiffJson(DiffEntries, Response);
	return true;
}
