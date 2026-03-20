// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Async/Async.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "EditorFramework/AssetImportData.h"
#include "FileHelpers.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/TextureFactory.h"
#include "GameMapsSettings.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/UnrealType.h"
#include "UObject/TopLevelAssetPath.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "WidgetBlueprintFactory.h"

#include "Containers/StringConv.h"
#include "HttpPath.h"
#include "HttpRouteHandle.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PluginDescriptor.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Types/SlateEnums.h"

#if PLATFORM_WINDOWS
#include "ILiveCodingModule.h"
#endif


DECLARE_LOG_CATEGORY_EXTERN(LogOctoMCP, Log, All);

namespace OctoMCP
{
	inline constexpr uint32 HttpPort = 47831;
	inline const TCHAR* const PluginName = TEXT("OctoMCP");
	inline const TCHAR* const FallbackPluginVersion = TEXT("0.2.0");
	inline const TCHAR* const HealthRoute = TEXT("/api/v1/health");
	inline const TCHAR* const CommandRoute = TEXT("/api/v1/command");
	inline const TCHAR* const CommandGetVersionInfo = TEXT("get_version_info");
	inline const TCHAR* const CommandLiveCodingCompile = TEXT("live_coding_compile");
	inline const TCHAR* const CommandCreateBlueprintAsset = TEXT("create_blueprint_asset");
	inline const TCHAR* const CommandCreateWidgetBlueprint = TEXT("create_widget_blueprint");
	inline const TCHAR* const CommandImportTextureAsset = TEXT("import_texture_asset");
	inline const TCHAR* const CommandAddWidgetBlueprintChildInstance = TEXT("add_widget_blueprint_child_instance");
	inline const TCHAR* const CommandSetUniformGridSlot = TEXT("set_uniform_grid_slot");
	inline const TCHAR* const CommandSyncUniformGridWidgetInstances = TEXT("sync_uniform_grid_widget_instances");
	inline const TCHAR* const CommandAddBlueprintInterface = TEXT("add_blueprint_interface");
	inline const TCHAR* const CommandConfigureTileView = TEXT("configure_tile_view");
	inline const TCHAR* const CommandReorderWidgetChild = TEXT("reorder_widget_child");
	inline const TCHAR* const CommandRemoveWidget = TEXT("remove_widget");
	inline const TCHAR* const CommandScaffoldWidgetBlueprint = TEXT("scaffold_widget_blueprint");
	inline const TCHAR* const CommandSetBlueprintClassProperty = TEXT("set_blueprint_class_property");
	inline const TCHAR* const CommandSetGlobalDefaultGameMode = TEXT("set_global_default_game_mode");
	inline const TCHAR* const CommandBootstrapProjectMap = TEXT("bootstrap_project_map");
	inline const TCHAR* const CommandSetWidgetBackgroundBlur = TEXT("set_widget_background_blur");
	inline const TCHAR* const CommandSetWidgetCornerRadius = TEXT("set_widget_corner_radius");
	inline const TCHAR* const CommandSetWidgetPanelColor = TEXT("set_widget_panel_color");
	inline const TCHAR* const CommandSetSizeBoxHeightOverride = TEXT("set_size_box_height_override");
	inline const TCHAR* const CommandSetPopupOpenElasticScale = TEXT("set_popup_open_elastic_scale");
	inline const TCHAR* const CommandSetWidgetImageTexture = TEXT("set_widget_image_texture");
	inline const TCHAR* const BootstrapTemplateMapPath = TEXT("/Engine/Maps/Templates/Template_Default");
	inline const TCHAR* const DefaultBootstrapLevelFileName = TEXT("BasicMap");
	inline const TCHAR* const DefaultBootstrapDirectoryPath = TEXT("/Game/Maps");
}

	struct FCreateWidgetBlueprintResult
	{
		bool bCreated = false;
		bool bSaved = false;
		bool bSuccess = false;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString ParentClassPath;
		FString ParentClassName;
	};

	struct FCreateBlueprintAssetResult
	{
		bool bCreated = false;
		bool bSaved = false;
		bool bSuccess = false;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString ParentClassPath;
		FString ParentClassName;
		FString GeneratedClassPath;
	};

	struct FScaffoldWidgetBlueprintResult
	{
		bool bSaved = false;
		bool bSuccess = false;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString ScaffoldType;
	};

	struct FImportTextureAssetResult
	{
		bool bImported = false;
		bool bSaved = false;
		bool bSuccess = false;
		FString Message;
		FString SourceFilePath;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
	};

	struct FSetBlueprintClassPropertyResult
	{
		bool bSaved = false;
		bool bSuccess = false;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString PropertyName;
		FString ValueClassPath;
		FString ValueClassName;
	};

	struct FSetWidgetImageTextureResult
	{
		bool bSaved = false;
		bool bSuccess = false;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString WidgetName;
		FString TextureAssetPath;
	};

	struct FSetWidgetBackgroundBlurResult
	{
		bool bConverted = false;
		bool bSaved = false;
		bool bSuccess = false;
		bool bApplyAlphaToBlur = false;
		bool bOverrideAutoRadiusCalculation = false;
		float BlurStrength = 0.0f;
		int32 BlurRadius = 0;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString WidgetName;
		FString WidgetClassName;
	};

	struct FSetWidgetCornerRadiusResult
	{
		bool bSaved = false;
		bool bSuccess = false;
		float Radius = 0.0f;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString WidgetName;
		FString WidgetClassName;
	};

	struct FSetWidgetPanelColorResult
	{
		bool bSaved = false;
		bool bSuccess = false;
		float Red = 0.0f;
		float Green = 0.0f;
		float Blue = 0.0f;
		float Alpha = 0.0f;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString WidgetName;
		FString WidgetClassName;
	};

	struct FSetPopupOpenElasticScaleResult
	{
		bool bSaved = false;
		bool bSuccess = false;
		bool bEnabled = false;
		float Duration = 0.0f;
		float StartScale = 1.0f;
		float OscillationCount = 0.0f;
		float PivotX = 0.5f;
		float PivotY = 0.5f;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString WidgetName;
	};

	struct FSetSizeBoxHeightOverrideResult
	{
		bool bSaved = false;
		bool bSuccess = false;
		float HeightOverride = 0.0f;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString WidgetName;
	};

	struct FAddWidgetBlueprintChildInstanceResult
	{
		bool bCreated = false;
		bool bReplaced = false;
		bool bSaved = false;
		bool bSuccess = false;
		int32 FinalIndex = INDEX_NONE;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString ParentWidgetName;
		FString ChildWidgetName;
		FString ChildWidgetAssetPath;
		FString ChildWidgetClassPath;
		FString ChildWidgetClassName;
	};

	struct FSetUniformGridSlotResult
	{
		bool bSaved = false;
		bool bSuccess = false;
		int32 Row = 0;
		int32 Column = 0;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString WidgetName;
		FString ParentWidgetName;
	};

	struct FSyncUniformGridWidgetInstancesResult
	{
		bool bSaved = false;
		bool bSuccess = false;
		bool bTrimManagedChildren = true;
		int32 Count = 0;
		int32 ColumnCount = 1;
		int32 CreatedCount = 0;
		int32 ReusedCount = 0;
		int32 RemovedCount = 0;
		int32 ManagedCount = 0;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString GridWidgetName;
		FString EntryWidgetAssetPath;
		FString EntryWidgetClassPath;
		FString EntryWidgetClassName;
		FString InstanceNamePrefix;
	};

	struct FAddBlueprintInterfaceResult
	{
		bool bAdded = false;
		bool bSaved = false;
		bool bSuccess = false;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString InterfaceClassPath;
		FString InterfaceClassName;
	};

	struct FConfigureTileViewResult
	{
		bool bSaved = false;
		bool bSuccess = false;
		float EntryWidth = 128.0f;
		float EntryHeight = 128.0f;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString WidgetName;
		FString EntryWidgetAssetPath;
		FString EntryWidgetClassPath;
		FString EntryWidgetClassName;
		FString Orientation;
	};

	struct FReorderWidgetChildResult
	{
		bool bReordered = false;
		bool bSaved = false;
		bool bSuccess = false;
		int32 RequestedIndex = INDEX_NONE;
		int32 FinalIndex = INDEX_NONE;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString WidgetName;
		FString ParentWidgetName;
	};

	struct FRemoveWidgetResult
	{
		bool bRemoved = false;
		bool bSaved = false;
		bool bSuccess = false;
		FString Message;
		FString AssetPath;
		FString AssetObjectPath;
		FString PackagePath;
		FString AssetName;
		FString WidgetName;
		FString ParentWidgetName;
	};

	struct FSetGlobalDefaultGameModeResult
	{
		bool bSaved = false;
		bool bSuccess = false;
		FString Message;
		FString GameModeClassPath;
	};

	struct FBootstrapProjectMapResult
	{
		bool bBootstrapped = false;
		bool bCreated = false;
		bool bForceCreate = false;
		bool bSavedConfig = false;
		bool bSuccess = false;
		int32 ExistingLevelCount = 0;
		FString Message;
		FString LevelFileName;
		FString DirectoryPath;
		FString LevelAssetPath;
		FString LevelObjectPath;
	};



class FOctoMCPModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void CachePluginVersion();

    void StartHttpBridge();

    void StopHttpBridge();

    bool HandleHealthRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) const;

    bool HandleCommandRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) const;

    TSharedRef<FJsonObject> BuildVersionInfoObject() const;

    TSharedRef<FJsonObject> BuildLiveCodingCompileObject(const bool bWaitForCompletion) const;

    TSharedRef<FJsonObject> BuildCreateBlueprintAssetObject(
    	const FString& AssetPath,
    	const FString& ParentClassPath,
    	const bool bSaveAsset) const;

    FCreateBlueprintAssetResult CreateBlueprintAsset(
    	const FString& InAssetPath,
    	const FString& InParentClassPath,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildCreateWidgetBlueprintObject(
    	const FString& AssetPath,
    	const FString& ParentClassPath,
    	const bool bSaveAsset) const;

    FCreateWidgetBlueprintResult CreateWidgetBlueprintAsset(
    	const FString& InAssetPath,
    	const FString& InParentClassPath,
    	const bool bSaveAsset) const;

    bool NormalizeWidgetBlueprintAssetPath(
    	const FString& InAssetPath,
    	FString& OutAssetPackageName,
    	FString& OutPackagePath,
    	FString& OutAssetName,
    	FString& OutAssetObjectPath,
    	FString& OutError) const;

    UClass* ResolveWidgetParentClass(
    	const FString& InParentClassPath,
    	FString& OutResolvedClassPath,
    	FString& OutError) const;

    UClass* ResolveClassReference(
    	const FString& InClassPath,
    	const UClass* RequiredBaseClass,
    	FString& OutResolvedClassPath,
    	FString& OutError) const;

    UTexture2D* ResolveTextureAsset(
    	const FString& InTextureAssetPath,
    	FString& OutResolvedAssetPath,
    	FString& OutError) const;

    UClass* ResolveWidgetBlueprintGeneratedClass(
    	const FString& InWidgetAssetPath,
    	FString& OutResolvedAssetPath,
    	FString& OutResolvedClassPath,
    	FString& OutError) const;

    UWidget* EnsureWidgetInstanceOfClass(
    	UWidgetBlueprint* WidgetBlueprint,
    	UClass* WidgetClass,
    	const TCHAR* WidgetName,
    	bool& bOutCreated,
    	bool& bOutReplaced,
    	FString& OutError) const;

    bool SetBoolPropertyValue(
    	UClass* const OwnerClass,
    	UObject* const ObjectInstance,
    	const TCHAR* PropertyName,
    	const bool bValue,
    	FString& OutError) const;

    bool SetFloatPropertyValue(
    	UClass* const OwnerClass,
    	UObject* const ObjectInstance,
    	const TCHAR* PropertyName,
    	const float Value,
    	FString& OutError) const;

    bool SetClassPropertyValue(
    	UClass* const OwnerClass,
    	UObject* const ObjectInstance,
    	const TCHAR* PropertyName,
    	UClass* const Value,
    	FString& OutError) const;

    bool SetNamePropertyValue(
    	UClass* const OwnerClass,
    	UObject* const ObjectInstance,
    	const TCHAR* PropertyName,
    	const FName Value,
    	FString& OutError) const;

    bool SetEnumPropertyValue(
    	UClass* const OwnerClass,
    	UObject* const ObjectInstance,
    	const TCHAR* PropertyName,
    	const int64 Value,
    	FString& OutError) const;

    bool SetVector2DPropertyValue(
    	UClass* const OwnerClass,
    	UObject* const ObjectInstance,
    	const TCHAR* PropertyName,
    	const FVector2D& Value,
    	FString& OutError) const;

    bool ParseOrientationValue(
    	const FString& InOrientation,
    	EOrientation& OutOrientation,
    	FString& OutNormalizedOrientation,
    	FString& OutError) const;

    TSharedRef<FJsonObject> BuildImportTextureAssetObject(
    	const FString& SourceFilePath,
    	const FString& AssetPath,
    	const bool bReplaceExisting,
    	const bool bSaveAsset) const;

    FImportTextureAssetResult ImportTextureAsset(
    	const FString& InSourceFilePath,
    	const FString& InAssetPath,
    	const bool bReplaceExisting,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildSetWidgetImageTextureObject(
    	const FString& AssetPath,
    	const FString& WidgetName,
    	const FString& TextureAssetPath,
    	const bool bMatchTextureSize,
    	const bool bSaveAsset) const;

    FSetWidgetImageTextureResult SetWidgetImageTexture(
    	const FString& InAssetPath,
    	const FString& InWidgetName,
    	const FString& InTextureAssetPath,
    	const bool bMatchTextureSize,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildSetWidgetBackgroundBlurObject(
    	const FString& AssetPath,
    	const FString& WidgetName,
    	const float BlurStrength,
    	const TOptional<int32> BlurRadius,
    	const bool bApplyAlphaToBlur,
    	const bool bSaveAsset) const;

    FSetWidgetBackgroundBlurResult SetWidgetBackgroundBlur(
    	const FString& InAssetPath,
    	const FString& InWidgetName,
    	const float InBlurStrength,
    	const TOptional<int32> InBlurRadius,
    	const bool bApplyAlphaToBlur,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildSetWidgetCornerRadiusObject(
    	const FString& AssetPath,
    	const FString& WidgetName,
    	const float Radius,
    	const bool bSaveAsset) const;

    FSetWidgetCornerRadiusResult SetWidgetCornerRadius(
    	const FString& InAssetPath,
    	const FString& InWidgetName,
    	const float InRadius,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildSetWidgetPanelColorObject(
    	const FString& AssetPath,
    	const FString& WidgetName,
    	const float Red,
    	const float Green,
    	const float Blue,
    	const float Alpha,
    	const bool bSaveAsset) const;

    FSetWidgetPanelColorResult SetWidgetPanelColor(
    	const FString& InAssetPath,
    	const FString& InWidgetName,
    	const float InRed,
    	const float InGreen,
    	const float InBlue,
    	const float InAlpha,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildSetSizeBoxHeightOverrideObject(
    	const FString& AssetPath,
    	const FString& WidgetName,
    	const float HeightOverride,
    	const bool bSaveAsset) const;

    FSetSizeBoxHeightOverrideResult SetSizeBoxHeightOverride(
    	const FString& InAssetPath,
    	const FString& InWidgetName,
    	const float InHeightOverride,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildSetPopupOpenElasticScaleObject(
    	const FString& AssetPath,
    	const bool bEnabled,
    	const FString& WidgetName,
    	const float Duration,
    	const float StartScale,
    	const float OscillationCount,
    	const float PivotX,
    	const float PivotY,
    	const bool bSaveAsset) const;

    FSetPopupOpenElasticScaleResult SetPopupOpenElasticScale(
    	const FString& InAssetPath,
    	const bool bEnabled,
    	const FString& InWidgetName,
    	const float InDuration,
    	const float InStartScale,
    	const float InOscillationCount,
    	const float InPivotX,
    	const float InPivotY,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildAddWidgetBlueprintChildInstanceObject(
    	const FString& AssetPath,
    	const FString& ParentWidgetName,
    	const FString& ChildWidgetAssetPath,
    	const FString& ChildWidgetName,
    	const int32 DesiredIndex,
    	const bool bSaveAsset) const;

    FAddWidgetBlueprintChildInstanceResult AddWidgetBlueprintChildInstance(
    	const FString& InAssetPath,
    	const FString& InParentWidgetName,
    	const FString& InChildWidgetAssetPath,
    	const FString& InChildWidgetName,
    	const int32 DesiredIndex,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildSetUniformGridSlotObject(
    	const FString& AssetPath,
    	const FString& WidgetName,
    	const int32 Row,
    	const int32 Column,
    	const bool bSaveAsset) const;

    FSetUniformGridSlotResult SetUniformGridSlot(
    	const FString& InAssetPath,
    	const FString& InWidgetName,
    	const int32 InRow,
    	const int32 InColumn,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildSyncUniformGridWidgetInstancesObject(
    	const FString& AssetPath,
    	const FString& GridWidgetName,
    	const FString& EntryWidgetAssetPath,
    	const int32 Count,
    	const int32 ColumnCount,
    	const FString& InstanceNamePrefix,
    	const bool bTrimManagedChildren,
    	const bool bSaveAsset) const;

    FSyncUniformGridWidgetInstancesResult SyncUniformGridWidgetInstances(
    	const FString& InAssetPath,
    	const FString& InGridWidgetName,
    	const FString& InEntryWidgetAssetPath,
    	const int32 InCount,
    	const int32 InColumnCount,
    	const FString& InInstanceNamePrefix,
    	const bool bTrimManagedChildren,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildAddBlueprintInterfaceObject(
    	const FString& AssetPath,
    	const FString& InterfaceClassPath,
    	const bool bSaveAsset) const;

    FAddBlueprintInterfaceResult AddBlueprintInterface(
    	const FString& InAssetPath,
    	const FString& InInterfaceClassPath,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildConfigureTileViewObject(
    	const FString& AssetPath,
    	const FString& WidgetName,
    	const FString& EntryWidgetAssetPath,
    	const float EntryWidth,
    	const float EntryHeight,
    	const FString& Orientation,
    	const bool bSaveAsset) const;

    FConfigureTileViewResult ConfigureTileView(
    	const FString& InAssetPath,
    	const FString& InWidgetName,
    	const FString& InEntryWidgetAssetPath,
    	const float InEntryWidth,
    	const float InEntryHeight,
    	const FString& InOrientation,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildReorderWidgetChildObject(
    	const FString& AssetPath,
    	const FString& WidgetName,
    	const int32 DesiredIndex,
    	const bool bSaveAsset) const;

    FReorderWidgetChildResult ReorderWidgetChild(
    	const FString& InAssetPath,
    	const FString& InWidgetName,
    	const int32 DesiredIndex,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildRemoveWidgetObject(
    	const FString& AssetPath,
    	const FString& WidgetName,
    	const bool bSaveAsset) const;

    FRemoveWidgetResult RemoveWidgetFromBlueprint(
    	const FString& InAssetPath,
    	const FString& InWidgetName,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildSetBlueprintClassPropertyObject(
    	const FString& AssetPath,
    	const FString& PropertyName,
    	const FString& ValueClassPath,
    	const bool bSaveAsset) const;

    FSetBlueprintClassPropertyResult SetBlueprintClassPropertyValue(
    	const FString& InAssetPath,
    	const FString& InPropertyName,
    	const FString& InValueClassPath,
    	const bool bSaveAsset) const;

    TSharedRef<FJsonObject> BuildSetGlobalDefaultGameModeObject(
    	const FString& GameModeClassPath,
    	const bool bSaveConfig) const;

    FSetGlobalDefaultGameModeResult SetGlobalDefaultGameMode(
    	const FString& InGameModeClassPath,
    	const bool bSaveConfig) const;

    TSharedRef<FJsonObject> BuildBootstrapProjectMapObject(
    	const FString& LevelFileName,
    	const FString& DirectoryPath,
    	const bool bForceCreate) const;

    FBootstrapProjectMapResult BootstrapProjectMap(
    	const FString& InLevelFileName,
    	const FString& InDirectoryPath,
    	const bool bForceCreate) const;

    TSharedRef<FJsonObject> BuildScaffoldWidgetBlueprintObject(
    	const FString& AssetPath,
    	const FString& ScaffoldType,
    	const bool bSaveAsset) const;

    FScaffoldWidgetBlueprintResult ScaffoldWidgetBlueprintAsset(
    	const FString& InAssetPath,
    	const FString& InScaffoldType,
    	const bool bSaveAsset) const;

    void ResetWidgetBlueprintTree(UWidgetBlueprint* WidgetBlueprint) const;

    void RegisterBindableWidget(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget) const;

    template <typename TWidget>
    TWidget* FindWidgetByName(UWidgetBlueprint* WidgetBlueprint, const TCHAR* WidgetName) const
    {
    	if (WidgetBlueprint == nullptr || WidgetBlueprint->WidgetTree == nullptr)
    	{
    		return nullptr;
    	}

    	return Cast<TWidget>(WidgetBlueprint->WidgetTree->FindWidget(FName(WidgetName)));
    }

    template <typename TWidget>
    TWidget* FindOrCreateWidget(UWidgetBlueprint* WidgetBlueprint, const TCHAR* WidgetName, bool& bOutCreated) const
    {
    	bOutCreated = false;

    	if (TWidget* const ExistingWidget = FindWidgetByName<TWidget>(WidgetBlueprint, WidgetName))
    	{
    		return ExistingWidget;
    	}

    	check(WidgetBlueprint != nullptr);
    	check(WidgetBlueprint->WidgetTree != nullptr);

    	bOutCreated = true;
    	return WidgetBlueprint->WidgetTree->ConstructWidget<TWidget>(TWidget::StaticClass(), FName(WidgetName));
    }

    bool RenameWidget(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget, const TCHAR* NewWidgetName) const;

    template <typename TRootWidget>
    TRootWidget* EnsureRootWidget(UWidgetBlueprint* WidgetBlueprint, const TCHAR* WidgetName, bool& bOutCreated) const
    {
    	check(WidgetBlueprint != nullptr);
    	check(WidgetBlueprint->WidgetTree != nullptr);

    	bOutCreated = false;

    	if (WidgetBlueprint->WidgetTree->RootWidget == nullptr)
    	{
    		bOutCreated = true;
    		TRootWidget* const RootWidget =
    			WidgetBlueprint->WidgetTree->ConstructWidget<TRootWidget>(TRootWidget::StaticClass(), FName(WidgetName));
    		WidgetBlueprint->WidgetTree->RootWidget = RootWidget;
    		return RootWidget;
    	}

    	TRootWidget* const RootWidget = Cast<TRootWidget>(WidgetBlueprint->WidgetTree->RootWidget);
    	if (RootWidget == nullptr)
    	{
    		return nullptr;
    	}

    	RenameWidget(WidgetBlueprint, RootWidget, WidgetName);
    	return RootWidget;
    }

    UPanelSlot* EnsureContent(UContentWidget* ParentWidget, UWidget* ChildWidget) const;

    UPanelSlot* EnsurePanelChildAt(UPanelWidget* ParentWidget, UWidget* ChildWidget, const int32 DesiredIndex) const;

    bool BuildPopupWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const;

    bool BuildBottomButtonBarWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const;

    bool BuildScrollUniformGridHostWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const;

    bool BuildTileViewHostWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const;

    bool BuildTileViewEntryWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const;

    bool BuildItemTilePopupWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const;

    bool BuildItemTileEntryWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const;

    FString BuildLiveCodingCompileMessage(
    	const ELiveCodingCompileResult CompileResult,
    	const FString& EnableErrorText) const;

    bool TryGetArgumentsObject(
    	const TSharedRef<FJsonObject>& RequestObject,
    	TSharedPtr<FJsonObject>& OutArgumentsObject,
    	FString& OutError) const;

    bool TryGetOptionalBoolArgument(
    	const TSharedPtr<FJsonObject>& ArgumentsObject,
    	const FString& FieldName,
    	bool& OutValue,
    	FString& OutError) const;

    bool TryGetOptionalStringArgument(
    	const TSharedPtr<FJsonObject>& ArgumentsObject,
    	const FString& FieldName,
    	FString& OutValue,
    	FString& OutError) const;

    bool TryGetOptionalIntArgument(
    	const TSharedPtr<FJsonObject>& ArgumentsObject,
    	const FString& FieldName,
    	int32& OutValue,
    	bool& bOutHasValue,
    	FString& OutError) const;

    bool TryGetOptionalFloatArgument(
    	const TSharedPtr<FJsonObject>& ArgumentsObject,
    	const FString& FieldName,
    	float& OutValue,
    	FString& OutError) const;

    bool TryGetRequiredIntArgument(
    	const TSharedPtr<FJsonObject>& ArgumentsObject,
    	const FString& FieldName,
    	int32& OutValue,
    	FString& OutError) const;

    bool TryGetRequiredFloatArgument(
    	const TSharedPtr<FJsonObject>& ArgumentsObject,
    	const FString& FieldName,
    	float& OutValue,
    	FString& OutError) const;

    bool TryGetRequiredStringArgument(
    	const TSharedPtr<FJsonObject>& ArgumentsObject,
    	const FString& FieldName,
    	FString& OutValue,
    	FString& OutError) const;

    bool TryParseJsonBody(const TArray<uint8>& Body, TSharedPtr<FJsonObject>& OutObject, FString& OutError) const;

    TUniquePtr<FHttpServerResponse> CreateJsonResponse(
    	const TSharedRef<FJsonObject>& JsonObject,
    	EHttpServerResponseCodes ResponseCode = EHttpServerResponseCodes::Ok) const;

    TUniquePtr<FHttpServerResponse> CreateErrorResponse(
    	EHttpServerResponseCodes ResponseCode,
    	const FString& ErrorCode,
    	const FString& ErrorMessage,
    	const FString& RequestId = FString()) const;

private:
    TSharedPtr<IHttpRouter> HttpRouter;
    FHttpRouteHandle HealthRouteHandle;
    FHttpRouteHandle CommandRouteHandle;
    FString PluginVersion;
};
