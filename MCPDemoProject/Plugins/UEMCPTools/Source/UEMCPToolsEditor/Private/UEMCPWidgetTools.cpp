#include "UEMCPWidgetTools.h"

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Layout/Visibility.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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
	const FString TargetName = FString(ManagedNamePrefix) + NormalizedKey;
	if (Widget->GetName() != TargetName)
	{
		Widget->Rename(*TargetName, Widget->GetOuter());
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
	}
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

	if (bDryRun)
	{
		Response.AppliedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("add_widget"), Key, TEXT("applied"), TEXT("Dry-run add"));
		return true;
	}

	WidgetBlueprint->Modify();
	WidgetBlueprint->WidgetTree->Modify();

	const FString DesiredName = Operation.WidgetName.IsEmpty() ? Key : Operation.WidgetName;
	UWidget* NewWidget = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, *DesiredName);
	SetManagedTags(NewWidget, Key);

	if (ParentWidget == nullptr)
	{
		WidgetBlueprint->WidgetTree->RootWidget = NewWidget;
	}
	else
	{
		UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
		if (ParentPanel == nullptr)
		{
			Response.SkippedCount += 1;
			Response.AddWarning(TEXT("skip_non_panel_parent"), TEXT("Parent widget is not a panel and cannot accept children."), Operation.ParentKey);
			AppendDiffEntry(DiffEntries, TEXT("add_widget"), Key, TEXT("skipped"), TEXT("Parent is not panel"));
			return true;
		}
		ParentPanel->Modify();
		ParentPanel->AddChild(NewWidget);
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
	if (WidgetBlueprint->WidgetTree->RootWidget == Widget)
	{
		WidgetBlueprint->WidgetTree->RootWidget = nullptr;
	}

	if (UPanelWidget* Parent = Widget->GetParent())
	{
		Parent->Modify();
		Parent->RemoveChild(Widget);
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

	UPanelWidget* NewParentPanel = Cast<UPanelWidget>(NewParent);
	if (NewParentPanel == nullptr)
	{
		Response.SkippedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("skipped"), TEXT("New parent is not a panel"));
		return true;
	}

	if (bDryRun)
	{
		Response.AppliedCount += 1;
		AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("applied"), TEXT("Dry-run reparent"));
		return true;
	}

	WidgetBlueprint->Modify();
	if (UPanelWidget* OldParent = Widget->GetParent())
	{
		OldParent->Modify();
		OldParent->RemoveChild(Widget);
	}

	NewParentPanel->Modify();
	NewParentPanel->AddChild(Widget);
	Response.AppliedCount += 1;
	AppendDiffEntry(DiffEntries, TEXT("reparent_widget"), Key, TEXT("applied"), TEXT("Widget reparented"));
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

	if (Operation.NumberProps.Contains(TEXT("render_opacity")))
	{
		bAnyChange = true;
		if (!bDryRun)
		{
			Widget->Modify();
			Widget->SetRenderOpacity(static_cast<float>(Operation.NumberProps[TEXT("render_opacity")]));
		}
	}

	if (!Operation.WidgetName.IsEmpty())
	{
		bAnyChange = true;
		if (!bDryRun)
		{
			Widget->Modify();
			Widget->Rename(*Operation.WidgetName, Widget->GetOuter());
		}
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
