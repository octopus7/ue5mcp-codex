// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/Async.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "EditorFramework/AssetImportData.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogOctoMCP, Log, All);

namespace OctoMCP
{
	constexpr uint32 HttpPort = 47831;
	const TCHAR* const PluginName = TEXT("OctoMCP");
	const TCHAR* const FallbackPluginVersion = TEXT("0.2.0");
	const TCHAR* const HealthRoute = TEXT("/api/v1/health");
	const TCHAR* const CommandRoute = TEXT("/api/v1/command");
	const TCHAR* const CommandGetVersionInfo = TEXT("get_version_info");
	const TCHAR* const CommandLiveCodingCompile = TEXT("live_coding_compile");
	const TCHAR* const CommandCreateBlueprintAsset = TEXT("create_blueprint_asset");
	const TCHAR* const CommandCreateWidgetBlueprint = TEXT("create_widget_blueprint");
	const TCHAR* const CommandImportTextureAsset = TEXT("import_texture_asset");
	const TCHAR* const CommandAddWidgetBlueprintChildInstance = TEXT("add_widget_blueprint_child_instance");
	const TCHAR* const CommandSetUniformGridSlot = TEXT("set_uniform_grid_slot");
	const TCHAR* const CommandSyncUniformGridWidgetInstances = TEXT("sync_uniform_grid_widget_instances");
	const TCHAR* const CommandAddBlueprintInterface = TEXT("add_blueprint_interface");
	const TCHAR* const CommandConfigureTileView = TEXT("configure_tile_view");
	const TCHAR* const CommandReorderWidgetChild = TEXT("reorder_widget_child");
	const TCHAR* const CommandRemoveWidget = TEXT("remove_widget");
	const TCHAR* const CommandScaffoldWidgetBlueprint = TEXT("scaffold_widget_blueprint");
	const TCHAR* const CommandSetBlueprintClassProperty = TEXT("set_blueprint_class_property");
	const TCHAR* const CommandSetGlobalDefaultGameMode = TEXT("set_global_default_game_mode");
	const TCHAR* const CommandSetWidgetBackgroundBlur = TEXT("set_widget_background_blur");
	const TCHAR* const CommandSetWidgetCornerRadius = TEXT("set_widget_corner_radius");
	const TCHAR* const CommandSetWidgetPanelColor = TEXT("set_widget_panel_color");
	const TCHAR* const CommandSetSizeBoxHeightOverride = TEXT("set_size_box_height_override");
	const TCHAR* const CommandSetPopupOpenElasticScale = TEXT("set_popup_open_elastic_scale");
	const TCHAR* const CommandSetWidgetImageTexture = TEXT("set_widget_image_texture");
}

namespace
{
#if WITH_LIVE_CODING
	const TCHAR* LexToString(const ELiveCodingCompileResult CompileResult)
	{
		switch (CompileResult)
		{
		case ELiveCodingCompileResult::Success:
			return TEXT("Success");
		case ELiveCodingCompileResult::NoChanges:
			return TEXT("NoChanges");
		case ELiveCodingCompileResult::InProgress:
			return TEXT("InProgress");
		case ELiveCodingCompileResult::CompileStillActive:
			return TEXT("CompileStillActive");
		case ELiveCodingCompileResult::NotStarted:
			return TEXT("NotStarted");
		case ELiveCodingCompileResult::Failure:
			return TEXT("Failure");
		case ELiveCodingCompileResult::Cancelled:
			return TEXT("Cancelled");
		default:
			return TEXT("Unknown");
		}
	}

#endif

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
}

class FOctoMCPModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		CachePluginVersion();
		StartHttpBridge();
	}

	virtual void ShutdownModule() override
	{
		StopHttpBridge();
	}

private:
	void CachePluginVersion()
	{
		PluginVersion = OctoMCP::FallbackPluginVersion;

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(OctoMCP::PluginName);
		if (!Plugin.IsValid())
		{
			return;
		}

		const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
		if (!Descriptor.VersionName.IsEmpty())
		{
			PluginVersion = Descriptor.VersionName;
			return;
		}

		PluginVersion = FString::FromInt(Descriptor.Version);
	}

	void StartHttpBridge()
	{
		FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
		HttpRouter = HttpServerModule.GetHttpRouter(OctoMCP::HttpPort, true);
		if (!HttpRouter.IsValid())
		{
			UE_LOG(LogOctoMCP, Error, TEXT("Failed to acquire HTTP router on port %u."), OctoMCP::HttpPort);
			return;
		}

		HealthRouteHandle = HttpRouter->BindRoute(
			FHttpPath(OctoMCP::HealthRoute),
			EHttpServerRequestVerbs::VERB_GET,
			FHttpRequestHandler::CreateRaw(this, &FOctoMCPModule::HandleHealthRequest));

		CommandRouteHandle = HttpRouter->BindRoute(
			FHttpPath(OctoMCP::CommandRoute),
			EHttpServerRequestVerbs::VERB_POST,
			FHttpRequestHandler::CreateRaw(this, &FOctoMCPModule::HandleCommandRequest));

		HttpServerModule.StartAllListeners();

		UE_LOG(
			LogOctoMCP,
			Log,
			TEXT("OctoMCP bridge ready at http://127.0.0.1:%u"),
			OctoMCP::HttpPort);
	}

	void StopHttpBridge()
	{
		if (HttpRouter.IsValid())
		{
			if (HealthRouteHandle.IsValid())
			{
				HttpRouter->UnbindRoute(HealthRouteHandle);
			}

			if (CommandRouteHandle.IsValid())
			{
				HttpRouter->UnbindRoute(CommandRouteHandle);
			}
		}

		HealthRouteHandle.Reset();
		CommandRouteHandle.Reset();
		HttpRouter.Reset();
	}

	bool HandleHealthRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) const
	{
		TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
		ResponseObject->SetBoolField(TEXT("ok"), true);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetStringField(TEXT("status"), TEXT("ok"));
		ResultObject->SetStringField(TEXT("pluginName"), OctoMCP::PluginName);
		ResultObject->SetStringField(TEXT("pluginVersion"), PluginVersion);
		ResultObject->SetStringField(TEXT("projectName"), FApp::GetProjectName());
		ResultObject->SetBoolField(TEXT("isEditor"), true);
		ResultObject->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
		ResultObject->SetStringField(TEXT("buildVersion"), FString(FApp::GetBuildVersion()));
		ResultObject->SetStringField(TEXT("route"), Request.RelativePath.GetPath());

		ResponseObject->SetObjectField(TEXT("result"), ResultObject);
		OnComplete(CreateJsonResponse(ResponseObject));
		return true;
	}

	bool HandleCommandRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) const
	{
		TSharedPtr<FJsonObject> RequestObject;
		FString BodyError;
		if (!TryParseJsonBody(Request.Body, RequestObject, BodyError))
		{
			OnComplete(CreateErrorResponse(EHttpServerResponseCodes::BadRequest, TEXT("invalid_json"), BodyError));
			return true;
		}

		FString RequestId;
		if (RequestObject->HasField(TEXT("requestId")) && !RequestObject->TryGetStringField(TEXT("requestId"), RequestId))
		{
			OnComplete(CreateErrorResponse(
				EHttpServerResponseCodes::BadRequest,
				TEXT("invalid_request_id"),
				TEXT("requestId must be a string when provided.")));
			return true;
		}

		FString Command;
		if (!RequestObject->TryGetStringField(TEXT("command"), Command) || Command.IsEmpty())
		{
			OnComplete(CreateErrorResponse(
				EHttpServerResponseCodes::BadRequest,
				TEXT("missing_command"),
				TEXT("Request body must include a non-empty command field."),
				RequestId));
			return true;
		}

		if (Command == OctoMCP::CommandGetVersionInfo)
		{
			TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
			ResponseObject->SetBoolField(TEXT("ok"), true);
			if (!RequestId.IsEmpty())
			{
				ResponseObject->SetStringField(TEXT("requestId"), RequestId);
			}
			ResponseObject->SetObjectField(TEXT("result"), BuildVersionInfoObject());

			OnComplete(CreateJsonResponse(ResponseObject));
			return true;
		}

		if (Command == OctoMCP::CommandLiveCodingCompile)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bWaitForCompletion = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("waitForCompletion"), bWaitForCompletion, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, bWaitForCompletion]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(TEXT("result"), BuildLiveCodingCompileObject(bWaitForCompletion));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandCreateBlueprintAsset)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString ParentClassPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("parentClassPath"), ParentClassPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, AssetPath, ParentClassPath, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(TEXT("result"), BuildCreateBlueprintAssetObject(AssetPath, ParentClassPath, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandCreateWidgetBlueprint)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString ParentClassPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("parentClassPath"), ParentClassPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, AssetPath, ParentClassPath, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(TEXT("result"), BuildCreateWidgetBlueprintObject(AssetPath, ParentClassPath, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandImportTextureAsset)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString SourceFilePath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("sourceFilePath"), SourceFilePath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bReplaceExisting = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("replaceExisting"), bReplaceExisting, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, SourceFilePath, AssetPath, bReplaceExisting, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildImportTextureAssetObject(SourceFilePath, AssetPath, bReplaceExisting, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandAddWidgetBlueprintChildInstance)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString ParentWidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("parentWidgetName"), ParentWidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString ChildWidgetAssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("childWidgetAssetPath"), ChildWidgetAssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString ChildWidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("childWidgetName"), ChildWidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			int32 DesiredIndex = INDEX_NONE;
			if (!TryGetRequiredIntArgument(ArgumentsObject, TEXT("desiredIndex"), DesiredIndex, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(
				ENamedThreads::GameThread,
				[this, CompletionCallback, CapturedRequestId, AssetPath, ParentWidgetName, ChildWidgetAssetPath, ChildWidgetName, DesiredIndex, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildAddWidgetBlueprintChildInstanceObject(
						AssetPath,
						ParentWidgetName,
						ChildWidgetAssetPath,
						ChildWidgetName,
						DesiredIndex,
						bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandSetUniformGridSlot)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString WidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("widgetName"), WidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			int32 Row = 0;
			if (!TryGetRequiredIntArgument(ArgumentsObject, TEXT("row"), Row, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			int32 Column = 0;
			if (!TryGetRequiredIntArgument(ArgumentsObject, TEXT("column"), Column, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, AssetPath, WidgetName, Row, Column, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildSetUniformGridSlotObject(AssetPath, WidgetName, Row, Column, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandSyncUniformGridWidgetInstances)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString GridWidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("gridWidgetName"), GridWidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString EntryWidgetAssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("entryWidgetAssetPath"), EntryWidgetAssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			int32 Count = 0;
			if (!TryGetRequiredIntArgument(ArgumentsObject, TEXT("count"), Count, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			int32 ColumnCount = 1;
			if (!TryGetRequiredIntArgument(ArgumentsObject, TEXT("columnCount"), ColumnCount, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString InstanceNamePrefix;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("instanceNamePrefix"), InstanceNamePrefix, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bTrimManagedChildren = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("trimManagedChildren"), bTrimManagedChildren, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(
				ENamedThreads::GameThread,
				[this, CompletionCallback, CapturedRequestId, AssetPath, GridWidgetName, EntryWidgetAssetPath, Count, ColumnCount, InstanceNamePrefix, bTrimManagedChildren, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildSyncUniformGridWidgetInstancesObject(
						AssetPath,
						GridWidgetName,
						EntryWidgetAssetPath,
						Count,
						ColumnCount,
						InstanceNamePrefix,
						bTrimManagedChildren,
						bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandAddBlueprintInterface)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString InterfaceClassPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("interfaceClassPath"), InterfaceClassPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, AssetPath, InterfaceClassPath, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildAddBlueprintInterfaceObject(AssetPath, InterfaceClassPath, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandConfigureTileView)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString WidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("widgetName"), WidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString EntryWidgetAssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("entryWidgetAssetPath"), EntryWidgetAssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float EntryWidth = 128.0f;
			if (!TryGetRequiredFloatArgument(ArgumentsObject, TEXT("entryWidth"), EntryWidth, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float EntryHeight = 128.0f;
			if (!TryGetRequiredFloatArgument(ArgumentsObject, TEXT("entryHeight"), EntryHeight, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString Orientation = TEXT("Vertical");
			if (ArgumentsObject.IsValid() && ArgumentsObject->HasField(TEXT("orientation")))
			{
				if (!ArgumentsObject->TryGetStringField(TEXT("orientation"), Orientation))
				{
					OnComplete(CreateErrorResponse(
						EHttpServerResponseCodes::BadRequest,
						TEXT("invalid_arguments"),
						TEXT("orientation must be a string when provided."),
						RequestId));
					return true;
				}

				Orientation = Orientation.TrimStartAndEnd();
				if (Orientation.IsEmpty())
				{
					OnComplete(CreateErrorResponse(
						EHttpServerResponseCodes::BadRequest,
						TEXT("invalid_arguments"),
						TEXT("orientation must not be empty when provided."),
						RequestId));
					return true;
				}
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(
				ENamedThreads::GameThread,
				[this, CompletionCallback, CapturedRequestId, AssetPath, WidgetName, EntryWidgetAssetPath, EntryWidth, EntryHeight, Orientation, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildConfigureTileViewObject(
						AssetPath,
						WidgetName,
						EntryWidgetAssetPath,
						EntryWidth,
						EntryHeight,
						Orientation,
						bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandSetWidgetBackgroundBlur)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString WidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("widgetName"), WidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float BlurStrength = 30.0f;
			if (!TryGetOptionalFloatArgument(ArgumentsObject, TEXT("blurStrength"), BlurStrength, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			int32 BlurRadius = 0;
			bool bHasBlurRadius = false;
			if (!TryGetOptionalIntArgument(ArgumentsObject, TEXT("blurRadius"), BlurRadius, bHasBlurRadius, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bApplyAlphaToBlur = false;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("applyAlphaToBlur"), bApplyAlphaToBlur, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(
				ENamedThreads::GameThread,
				[this, CompletionCallback, CapturedRequestId, AssetPath, WidgetName, BlurStrength, BlurRadius, bHasBlurRadius, bApplyAlphaToBlur, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildSetWidgetBackgroundBlurObject(
						AssetPath,
						WidgetName,
						BlurStrength,
						bHasBlurRadius ? TOptional<int32>(BlurRadius) : TOptional<int32>(),
						bApplyAlphaToBlur,
						bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandSetWidgetCornerRadius)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString WidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("widgetName"), WidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float Radius = 0.0f;
			if (!TryGetRequiredFloatArgument(ArgumentsObject, TEXT("radius"), Radius, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, AssetPath, WidgetName, Radius, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildSetWidgetCornerRadiusObject(AssetPath, WidgetName, Radius, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandScaffoldWidgetBlueprint)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString ScaffoldType;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("scaffoldType"), ScaffoldType, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, AssetPath, ScaffoldType, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(TEXT("result"), BuildScaffoldWidgetBlueprintObject(AssetPath, ScaffoldType, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandReorderWidgetChild)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString WidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("widgetName"), WidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			int32 DesiredIndex = INDEX_NONE;
			if (!TryGetRequiredIntArgument(ArgumentsObject, TEXT("desiredIndex"), DesiredIndex, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(
				ENamedThreads::GameThread,
				[this, CompletionCallback, CapturedRequestId, AssetPath, WidgetName, DesiredIndex, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildReorderWidgetChildObject(AssetPath, WidgetName, DesiredIndex, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandRemoveWidget)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString WidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("widgetName"), WidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, AssetPath, WidgetName, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildRemoveWidgetObject(AssetPath, WidgetName, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandSetWidgetPanelColor)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString WidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("widgetName"), WidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float Red = 0.0f;
			if (!TryGetRequiredFloatArgument(ArgumentsObject, TEXT("red"), Red, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float Green = 0.0f;
			if (!TryGetRequiredFloatArgument(ArgumentsObject, TEXT("green"), Green, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float Blue = 0.0f;
			if (!TryGetRequiredFloatArgument(ArgumentsObject, TEXT("blue"), Blue, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float Alpha = 1.0f;
			if (!TryGetRequiredFloatArgument(ArgumentsObject, TEXT("alpha"), Alpha, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(
				ENamedThreads::GameThread,
				[this, CompletionCallback, CapturedRequestId, AssetPath, WidgetName, Red, Green, Blue, Alpha, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildSetWidgetPanelColorObject(AssetPath, WidgetName, Red, Green, Blue, Alpha, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandSetSizeBoxHeightOverride)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString WidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("widgetName"), WidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float HeightOverride = 0.0f;
			if (!TryGetRequiredFloatArgument(ArgumentsObject, TEXT("heightOverride"), HeightOverride, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(
				ENamedThreads::GameThread,
				[this, CompletionCallback, CapturedRequestId, AssetPath, WidgetName, HeightOverride, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildSetSizeBoxHeightOverrideObject(AssetPath, WidgetName, HeightOverride, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandSetPopupOpenElasticScale)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bEnabled = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("enabled"), bEnabled, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString WidgetName = TEXT("PopupCard");
			if (ArgumentsObject.IsValid() && ArgumentsObject->HasField(TEXT("widgetName")))
			{
				if (!ArgumentsObject->TryGetStringField(TEXT("widgetName"), WidgetName))
				{
					OnComplete(CreateErrorResponse(
						EHttpServerResponseCodes::BadRequest,
						TEXT("invalid_arguments"),
						TEXT("widgetName must be a string when provided."),
						RequestId));
					return true;
				}

				WidgetName = WidgetName.TrimStartAndEnd();
				if (WidgetName.IsEmpty())
				{
					OnComplete(CreateErrorResponse(
						EHttpServerResponseCodes::BadRequest,
						TEXT("invalid_arguments"),
						TEXT("widgetName must not be empty when provided."),
						RequestId));
					return true;
				}
			}

			float Duration = 0.45f;
			if (!TryGetOptionalFloatArgument(ArgumentsObject, TEXT("duration"), Duration, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float StartScale = 0.82f;
			if (!TryGetOptionalFloatArgument(ArgumentsObject, TEXT("startScale"), StartScale, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float OscillationCount = 2.0f;
			if (!TryGetOptionalFloatArgument(ArgumentsObject, TEXT("oscillationCount"), OscillationCount, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float PivotX = 0.5f;
			if (!TryGetOptionalFloatArgument(ArgumentsObject, TEXT("pivotX"), PivotX, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			float PivotY = 0.5f;
			if (!TryGetOptionalFloatArgument(ArgumentsObject, TEXT("pivotY"), PivotY, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(
				ENamedThreads::GameThread,
				[this, CompletionCallback, CapturedRequestId, AssetPath, bEnabled, WidgetName, Duration, StartScale, OscillationCount, PivotX, PivotY, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildSetPopupOpenElasticScaleObject(
						AssetPath,
						bEnabled,
						WidgetName,
						Duration,
						StartScale,
						OscillationCount,
						PivotX,
						PivotY,
						bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandSetWidgetImageTexture)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString WidgetName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("widgetName"), WidgetName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString TextureAssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("textureAssetPath"), TextureAssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bMatchTextureSize = false;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("matchTextureSize"), bMatchTextureSize, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(
				ENamedThreads::GameThread,
				[this, CompletionCallback, CapturedRequestId, AssetPath, WidgetName, TextureAssetPath, bMatchTextureSize, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildSetWidgetImageTextureObject(AssetPath, WidgetName, TextureAssetPath, bMatchTextureSize, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandSetBlueprintClassProperty)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString AssetPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("assetPath"), AssetPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString PropertyName;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("propertyName"), PropertyName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString ValueClassPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("valueClassPath"), ValueClassPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveAsset = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveAsset"), bSaveAsset, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, AssetPath, PropertyName, ValueClassPath, bSaveAsset]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildSetBlueprintClassPropertyObject(AssetPath, PropertyName, ValueClassPath, bSaveAsset));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		if (Command == OctoMCP::CommandSetGlobalDefaultGameMode)
		{
			TSharedPtr<FJsonObject> ArgumentsObject;
			if (!TryGetArgumentsObject(RequestObject.ToSharedRef(), ArgumentsObject, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString GameModeClassPath;
			if (!TryGetRequiredStringArgument(ArgumentsObject, TEXT("gameModeClassPath"), GameModeClassPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bSaveConfig = true;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("saveConfig"), bSaveConfig, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			const FHttpResultCallback CompletionCallback = OnComplete;
			const FString CapturedRequestId = RequestId;
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, GameModeClassPath, bSaveConfig]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildSetGlobalDefaultGameModeObject(GameModeClassPath, bSaveConfig));

				CompletionCallback(CreateJsonResponse(ResponseObject));
			});
			return true;
		}

		OnComplete(CreateErrorResponse(
			EHttpServerResponseCodes::BadRequest,
			TEXT("unknown_command"),
			FString::Printf(TEXT("Unsupported command: %s"), *Command),
			RequestId));
		return true;
	}

	TSharedRef<FJsonObject> BuildVersionInfoObject() const
	{
		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
		ResultObject->SetStringField(TEXT("buildVersion"), FString(FApp::GetBuildVersion()));
		ResultObject->SetStringField(TEXT("projectName"), FApp::GetProjectName());
		ResultObject->SetStringField(TEXT("pluginVersion"), PluginVersion);
		ResultObject->SetBoolField(TEXT("isEditor"), true);
		return ResultObject;
	}

	TSharedRef<FJsonObject> BuildLiveCodingCompileObject(const bool bWaitForCompletion) const
	{
		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("liveCodingSupported"), false);
		ResultObject->SetBoolField(TEXT("waitedForCompletion"), bWaitForCompletion);
		ResultObject->SetBoolField(TEXT("success"), false);
		ResultObject->SetBoolField(TEXT("enabledByDefault"), false);
		ResultObject->SetBoolField(TEXT("canEnableForSession"), false);
		ResultObject->SetBoolField(TEXT("enabledForSession"), false);
		ResultObject->SetBoolField(TEXT("hasStarted"), false);
		ResultObject->SetBoolField(TEXT("isCompiling"), false);
		ResultObject->SetStringField(TEXT("compileResult"), TEXT("Unsupported"));
		ResultObject->SetStringField(TEXT("message"), TEXT("Live Coding is not available in this editor build."));
		ResultObject->SetStringField(TEXT("enableError"), TEXT(""));

#if WITH_LIVE_CODING
		ILiveCodingModule* const LiveCoding = FModuleManager::LoadModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
		if (LiveCoding == nullptr)
		{
			ResultObject->SetStringField(TEXT("compileResult"), TEXT("ModuleUnavailable"));
			ResultObject->SetStringField(TEXT("message"), TEXT("Live Coding module is not available in this editor session."));
			return ResultObject;
		}

		ResultObject->SetBoolField(TEXT("liveCodingSupported"), true);
		ResultObject->SetBoolField(TEXT("enabledByDefault"), LiveCoding->IsEnabledByDefault());
		ResultObject->SetBoolField(TEXT("canEnableForSession"), LiveCoding->CanEnableForSession());

		const ELiveCodingCompileFlags CompileFlags = bWaitForCompletion
			? ELiveCodingCompileFlags::WaitForCompletion
			: ELiveCodingCompileFlags::None;

		ELiveCodingCompileResult CompileResult = ELiveCodingCompileResult::Failure;
		const bool bCompileSucceeded = LiveCoding->Compile(CompileFlags, &CompileResult);
		const FString EnableErrorText = LiveCoding->GetEnableErrorText().ToString();

		ResultObject->SetBoolField(TEXT("success"), bCompileSucceeded);
		ResultObject->SetBoolField(TEXT("enabledForSession"), LiveCoding->IsEnabledForSession());
		ResultObject->SetBoolField(TEXT("hasStarted"), LiveCoding->HasStarted());
		ResultObject->SetBoolField(TEXT("isCompiling"), LiveCoding->IsCompiling());
		ResultObject->SetStringField(TEXT("compileResult"), LexToString(CompileResult));
		ResultObject->SetStringField(TEXT("enableError"), EnableErrorText);
		ResultObject->SetStringField(TEXT("message"), BuildLiveCodingCompileMessage(CompileResult, EnableErrorText));
#endif

		return ResultObject;
	}

	TSharedRef<FJsonObject> BuildCreateBlueprintAssetObject(
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

	FCreateBlueprintAssetResult CreateBlueprintAsset(
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

	TSharedRef<FJsonObject> BuildCreateWidgetBlueprintObject(
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

	FCreateWidgetBlueprintResult CreateWidgetBlueprintAsset(
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

	bool NormalizeWidgetBlueprintAssetPath(
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

	UClass* ResolveWidgetParentClass(
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

	UClass* ResolveClassReference(
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

	UTexture2D* ResolveTextureAsset(
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

	UClass* ResolveWidgetBlueprintGeneratedClass(
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

	UWidget* EnsureWidgetInstanceOfClass(
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

	bool SetBoolPropertyValue(
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

	bool SetFloatPropertyValue(
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

	bool SetClassPropertyValue(
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

	bool SetNamePropertyValue(
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

	bool SetEnumPropertyValue(
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

	bool SetVector2DPropertyValue(
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

	bool ParseOrientationValue(
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

	TSharedRef<FJsonObject> BuildImportTextureAssetObject(
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

	FImportTextureAssetResult ImportTextureAsset(
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

	TSharedRef<FJsonObject> BuildSetWidgetImageTextureObject(
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

	FSetWidgetImageTextureResult SetWidgetImageTexture(
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

	TSharedRef<FJsonObject> BuildSetWidgetBackgroundBlurObject(
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

	FSetWidgetBackgroundBlurResult SetWidgetBackgroundBlur(
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

	TSharedRef<FJsonObject> BuildSetWidgetCornerRadiusObject(
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

	FSetWidgetCornerRadiusResult SetWidgetCornerRadius(
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

	TSharedRef<FJsonObject> BuildSetWidgetPanelColorObject(
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

	FSetWidgetPanelColorResult SetWidgetPanelColor(
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

	TSharedRef<FJsonObject> BuildSetSizeBoxHeightOverrideObject(
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

	FSetSizeBoxHeightOverrideResult SetSizeBoxHeightOverride(
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

	TSharedRef<FJsonObject> BuildSetPopupOpenElasticScaleObject(
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

	FSetPopupOpenElasticScaleResult SetPopupOpenElasticScale(
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

	TSharedRef<FJsonObject> BuildAddWidgetBlueprintChildInstanceObject(
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

	FAddWidgetBlueprintChildInstanceResult AddWidgetBlueprintChildInstance(
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

	TSharedRef<FJsonObject> BuildSetUniformGridSlotObject(
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

	FSetUniformGridSlotResult SetUniformGridSlot(
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

	TSharedRef<FJsonObject> BuildSyncUniformGridWidgetInstancesObject(
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

	FSyncUniformGridWidgetInstancesResult SyncUniformGridWidgetInstances(
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

	TSharedRef<FJsonObject> BuildAddBlueprintInterfaceObject(
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

	FAddBlueprintInterfaceResult AddBlueprintInterface(
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

	TSharedRef<FJsonObject> BuildConfigureTileViewObject(
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

	FConfigureTileViewResult ConfigureTileView(
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

	TSharedRef<FJsonObject> BuildReorderWidgetChildObject(
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

	FReorderWidgetChildResult ReorderWidgetChild(
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

	TSharedRef<FJsonObject> BuildRemoveWidgetObject(
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

	FRemoveWidgetResult RemoveWidgetFromBlueprint(
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

	TSharedRef<FJsonObject> BuildSetBlueprintClassPropertyObject(
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

	FSetBlueprintClassPropertyResult SetBlueprintClassPropertyValue(
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

	TSharedRef<FJsonObject> BuildSetGlobalDefaultGameModeObject(
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

	FSetGlobalDefaultGameModeResult SetGlobalDefaultGameMode(
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

	TSharedRef<FJsonObject> BuildScaffoldWidgetBlueprintObject(
		const FString& AssetPath,
		const FString& ScaffoldType,
		const bool bSaveAsset) const
	{
		const FScaffoldWidgetBlueprintResult ScaffoldResult =
			ScaffoldWidgetBlueprintAsset(AssetPath, ScaffoldType, bSaveAsset);

		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetBoolField(TEXT("saved"), ScaffoldResult.bSaved);
		ResultObject->SetBoolField(TEXT("success"), ScaffoldResult.bSuccess);
		ResultObject->SetStringField(TEXT("message"), ScaffoldResult.Message);
		ResultObject->SetStringField(TEXT("assetPath"), ScaffoldResult.AssetPath);
		ResultObject->SetStringField(TEXT("assetObjectPath"), ScaffoldResult.AssetObjectPath);
		ResultObject->SetStringField(TEXT("packagePath"), ScaffoldResult.PackagePath);
		ResultObject->SetStringField(TEXT("assetName"), ScaffoldResult.AssetName);
		ResultObject->SetStringField(TEXT("scaffoldType"), ScaffoldResult.ScaffoldType);
		return ResultObject;
	}

	FScaffoldWidgetBlueprintResult ScaffoldWidgetBlueprintAsset(
		const FString& InAssetPath,
		const FString& InScaffoldType,
		const bool bSaveAsset) const
	{
		FScaffoldWidgetBlueprintResult Result;

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
		Result.ScaffoldType = InScaffoldType.TrimStartAndEnd().ToLower();

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

		WidgetBlueprint->SetFlags(RF_Transactional);
		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->SetFlags(RF_Transactional);
		WidgetBlueprint->WidgetTree->Modify();

		bool bBuiltScaffold = false;
		if (Result.ScaffoldType == TEXT("popup"))
		{
			bBuiltScaffold = BuildPopupWidgetScaffold(WidgetBlueprint);
		}
		else if (Result.ScaffoldType == TEXT("bottom_button_bar"))
		{
			bBuiltScaffold = BuildBottomButtonBarWidgetScaffold(WidgetBlueprint);
		}
		else if (Result.ScaffoldType == TEXT("scroll_uniform_grid_host"))
		{
			bBuiltScaffold = BuildScrollUniformGridHostWidgetScaffold(WidgetBlueprint);
		}
		else if (Result.ScaffoldType == TEXT("tile_view_host"))
		{
			bBuiltScaffold = BuildTileViewHostWidgetScaffold(WidgetBlueprint);
		}
		else if (Result.ScaffoldType == TEXT("tile_view_entry"))
		{
			bBuiltScaffold = BuildTileViewEntryWidgetScaffold(WidgetBlueprint);
		}
		else
		{
			Result.Message = FString::Printf(TEXT("Unsupported scaffoldType: %s"), *InScaffoldType);
			return Result;
		}

		if (!bBuiltScaffold || WidgetBlueprint->WidgetTree->RootWidget == nullptr)
		{
			Result.Message = FString::Printf(
				TEXT("Failed to build scaffold %s for Widget Blueprint %s."),
				*Result.ScaffoldType,
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
					TEXT("Scaffolded Widget Blueprint but could not access the EditorAssetSubsystem to save it: %s"),
					*AssetObjectPath);
				return Result;
			}

			Result.bSaved = EditorAssetSubsystem->SaveLoadedAsset(WidgetBlueprint, false);
			if (!Result.bSaved)
			{
				Result.Message = FString::Printf(
					TEXT("Scaffolded Widget Blueprint but failed to save it: %s"),
					*AssetObjectPath);
				return Result;
			}
		}

		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Scaffolded Widget Blueprint %s using scaffold type %s."),
			*AssetObjectPath,
			*Result.ScaffoldType);
		return Result;
	}

	void ResetWidgetBlueprintTree(UWidgetBlueprint* WidgetBlueprint) const
	{
		check(WidgetBlueprint != nullptr);
		check(WidgetBlueprint->WidgetTree != nullptr);

		TArray<UWidget*> ExistingWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(ExistingWidgets);

		for (UWidget* Widget : ExistingWidgets)
		{
			if (Widget != nullptr && Widget->bIsVariable)
			{
				WidgetBlueprint->OnVariableRemoved(Widget->GetFName());
			}
		}

		for (int32 WidgetIndex = ExistingWidgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
		{
			if (ExistingWidgets[WidgetIndex] != nullptr)
			{
				WidgetBlueprint->WidgetTree->RemoveWidget(ExistingWidgets[WidgetIndex]);
			}
		}

		WidgetBlueprint->WidgetTree->NamedSlotBindings.Empty();
		WidgetBlueprint->WidgetTree->RootWidget = nullptr;
	}

	void RegisterBindableWidget(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget) const
	{
		check(WidgetBlueprint != nullptr);
		check(Widget != nullptr);

		if (!Widget->bIsVariable)
		{
			Widget->bIsVariable = true;
			WidgetBlueprint->OnVariableAdded(Widget->GetFName());
		}
	}

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

	bool RenameWidget(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget, const TCHAR* NewWidgetName) const
	{
		check(WidgetBlueprint != nullptr);
		check(WidgetBlueprint->WidgetTree != nullptr);
		check(Widget != nullptr);

		const FName TargetName(NewWidgetName);
		if (Widget->GetFName() == TargetName)
		{
			return true;
		}

		if (UWidget* const ExistingWidget = WidgetBlueprint->WidgetTree->FindWidget(TargetName))
		{
			return ExistingWidget == Widget;
		}

		const FName PreviousName = Widget->GetFName();
		const bool bWasVariable = Widget->bIsVariable;
		if (bWasVariable)
		{
			WidgetBlueprint->OnVariableRemoved(PreviousName);
		}

		const bool bRenamed = Widget->Rename(
			*TargetName.ToString(),
			WidgetBlueprint->WidgetTree,
			REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
		if (!bRenamed)
		{
			if (bWasVariable)
			{
				WidgetBlueprint->OnVariableAdded(PreviousName);
			}
			return false;
		}

		if (bWasVariable)
		{
			WidgetBlueprint->OnVariableAdded(TargetName);
		}

		return true;
	}

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

	UPanelSlot* EnsureContent(UContentWidget* ParentWidget, UWidget* ChildWidget) const
	{
		if (ParentWidget == nullptr || ChildWidget == nullptr)
		{
			return nullptr;
		}

		if (ParentWidget->GetContent() == ChildWidget)
		{
			return ChildWidget->Slot;
		}

		if (UPanelWidget* const ExistingParent = ChildWidget->GetParent())
		{
			ExistingParent->RemoveChild(ChildWidget);
		}

		return ParentWidget->SetContent(ChildWidget);
	}

	UPanelSlot* EnsurePanelChildAt(UPanelWidget* ParentWidget, UWidget* ChildWidget, const int32 DesiredIndex) const
	{
		if (ParentWidget == nullptr || ChildWidget == nullptr)
		{
			return nullptr;
		}

		UPanelSlot* SlotTemplate = ChildWidget->Slot;
		if (SlotTemplate != nullptr && !SlotTemplate->IsA(ParentWidget->GetSlotClass()))
		{
			SlotTemplate = nullptr;
		}

		if (UPanelWidget* const ExistingParent = ChildWidget->GetParent())
		{
			const int32 ExistingIndex = ExistingParent->GetChildIndex(ChildWidget);
			const int32 ClampedDesiredIndex = FMath::Clamp(DesiredIndex, 0, ParentWidget->GetChildrenCount());
			if (ExistingParent == ParentWidget && ExistingIndex == ClampedDesiredIndex)
			{
				return ChildWidget->Slot;
			}

			ExistingParent->RemoveChild(ChildWidget);
		}

		const int32 InsertIndex = FMath::Clamp(DesiredIndex, 0, ParentWidget->GetChildrenCount());
		if (InsertIndex >= ParentWidget->GetChildrenCount())
		{
			return SlotTemplate != nullptr
				? ParentWidget->AddChild(ChildWidget, SlotTemplate)
				: ParentWidget->AddChild(ChildWidget);
		}

		return SlotTemplate != nullptr
			? ParentWidget->InsertChildAt(InsertIndex, ChildWidget, SlotTemplate)
			: ParentWidget->InsertChildAt(InsertIndex, ChildWidget);
	}

	bool BuildPopupWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
	{
		check(WidgetBlueprint != nullptr);
		check(WidgetBlueprint->WidgetTree != nullptr);

		bool bCreatedRootCanvas = false;
		UCanvasPanel* const RootCanvas = EnsureRootWidget<UCanvasPanel>(WidgetBlueprint, TEXT("RootCanvas"), bCreatedRootCanvas);
		if (RootCanvas == nullptr)
		{
			return false;
		}

		bool bCreatedBackdrop = false;
		UBorder* const Backdrop = FindOrCreateWidget<UBorder>(WidgetBlueprint, TEXT("Backdrop"), bCreatedBackdrop);
		if (Backdrop == nullptr)
		{
			return false;
		}

		if (bCreatedBackdrop)
		{
			Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.58f));
		}

		if (UCanvasPanelSlot* const BackdropSlot = Cast<UCanvasPanelSlot>(EnsurePanelChildAt(RootCanvas, Backdrop, 0)))
		{
			if (bCreatedBackdrop)
			{
				BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				BackdropSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
				BackdropSlot->SetZOrder(0);
			}
		}

		bool bCreatedPopupCard = false;
		UContentWidget* PopupCard = FindWidgetByName<UBackgroundBlur>(WidgetBlueprint, TEXT("PopupCard"));
		if (PopupCard == nullptr)
		{
			PopupCard = FindWidgetByName<UBorder>(WidgetBlueprint, TEXT("PopupCard"));
		}
		if (PopupCard == nullptr)
		{
			bCreatedPopupCard = true;
			PopupCard = WidgetBlueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PopupCard"));
		}
		if (PopupCard == nullptr)
		{
			return false;
		}

		if (bCreatedPopupCard)
		{
			if (UBorder* const PopupCardBorder = Cast<UBorder>(PopupCard))
			{
				PopupCardBorder->SetPadding(FMargin(28.0f));
				PopupCardBorder->SetHorizontalAlignment(HAlign_Fill);
				PopupCardBorder->SetVerticalAlignment(VAlign_Fill);
				PopupCardBorder->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.11f, 1.0f));
			}
			else if (UBackgroundBlur* const PopupCardBlur = Cast<UBackgroundBlur>(PopupCard))
			{
				PopupCardBlur->SetPadding(FMargin(28.0f));
				PopupCardBlur->SetHorizontalAlignment(HAlign_Fill);
				PopupCardBlur->SetVerticalAlignment(VAlign_Fill);
			}
		}

		if (UCanvasPanelSlot* const PopupCardSlot = Cast<UCanvasPanelSlot>(EnsurePanelChildAt(RootCanvas, PopupCard, 1)))
		{
			if (bCreatedPopupCard)
			{
				PopupCardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
				PopupCardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				PopupCardSlot->SetOffsets(FMargin(0.0f, 0.0f, 560.0f, 640.0f));
				PopupCardSlot->SetZOrder(1);
			}
		}

		bool bCreatedPopupContent = false;
		UVerticalBox* const PopupContent = FindOrCreateWidget<UVerticalBox>(WidgetBlueprint, TEXT("PopupContent"), bCreatedPopupContent);
		if (PopupContent == nullptr || EnsureContent(PopupCard, PopupContent) == nullptr)
		{
			return false;
		}

		bool bCreatedHeaderRow = false;
		UHorizontalBox* const HeaderRow = FindOrCreateWidget<UHorizontalBox>(WidgetBlueprint, TEXT("HeaderRow"), bCreatedHeaderRow);
		if (HeaderRow == nullptr)
		{
			return false;
		}

		if (UVerticalBoxSlot* const HeaderSlot = Cast<UVerticalBoxSlot>(EnsurePanelChildAt(PopupContent, HeaderRow, 0)))
		{
			if (bCreatedHeaderRow)
			{
				HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
				HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
				HeaderSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		if (UTextBlock* const LegacyContentTitleText = FindWidgetByName<UTextBlock>(WidgetBlueprint, TEXT("TitleText")))
		{
			if (LegacyContentTitleText->GetParent() == PopupContent)
			{
				TSet<UWidget*> WidgetsToDelete;
				WidgetsToDelete.Add(LegacyContentTitleText);
				FWidgetBlueprintEditorUtils::DeleteWidgets(
					WidgetBlueprint,
					WidgetsToDelete,
					FWidgetBlueprintEditorUtils::EDeleteWidgetWarningType::DeleteSilently);
			}
		}

		UTextBlock* TitleText = FindWidgetByName<UTextBlock>(WidgetBlueprint, TEXT("TitleText"));
		if (TitleText == nullptr)
		{
			if (UTextBlock* const LegacyHeaderText = FindWidgetByName<UTextBlock>(WidgetBlueprint, TEXT("HeaderText")))
			{
				if (LegacyHeaderText->GetParent() == HeaderRow)
				{
					if (!RenameWidget(WidgetBlueprint, LegacyHeaderText, TEXT("TitleText")))
					{
						return false;
					}
					TitleText = FindWidgetByName<UTextBlock>(WidgetBlueprint, TEXT("TitleText"));
				}
			}
		}

		bool bCreatedTitleText = false;
		if (TitleText == nullptr)
		{
			TitleText = FindOrCreateWidget<UTextBlock>(WidgetBlueprint, TEXT("TitleText"), bCreatedTitleText);
		}

		if (TitleText == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, TitleText);

		if (bCreatedTitleText)
		{
			TitleText->SetText(FText::FromString(TEXT("Popup Title")));
			TitleText->SetAutoWrapText(true);
			TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}

		if (UHorizontalBoxSlot* const HeaderTextSlot = Cast<UHorizontalBoxSlot>(EnsurePanelChildAt(HeaderRow, TitleText, 0)))
		{
			if (bCreatedTitleText)
			{
				FSlateChildSize FillSize(ESlateSizeRule::Fill);
				FillSize.Value = 1.0f;
				HeaderTextSlot->SetSize(FillSize);
				HeaderTextSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
				HeaderTextSlot->SetHorizontalAlignment(HAlign_Left);
				HeaderTextSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		bool bCreatedCloseButton = false;
		UButton* const CloseButton = FindOrCreateWidget<UButton>(WidgetBlueprint, TEXT("CloseButton"), bCreatedCloseButton);
		if (CloseButton == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, CloseButton);
		if (bCreatedCloseButton)
		{
			CloseButton->SetBackgroundColor(FLinearColor(0.16f, 0.17f, 0.20f, 1.0f));
		}

		if (UHorizontalBoxSlot* const CloseSlot = Cast<UHorizontalBoxSlot>(EnsurePanelChildAt(HeaderRow, CloseButton, 1)))
		{
			if (bCreatedCloseButton)
			{
				CloseSlot->SetHorizontalAlignment(HAlign_Right);
				CloseSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		bool bCreatedCloseLabel = false;
		UTextBlock* const CloseLabel = FindOrCreateWidget<UTextBlock>(WidgetBlueprint, TEXT("CloseLabel"), bCreatedCloseLabel);
		if (CloseLabel == nullptr || EnsureContent(CloseButton, CloseLabel) == nullptr)
		{
			return false;
		}

		if (bCreatedCloseLabel)
		{
			CloseLabel->SetText(FText::FromString(TEXT("Close")));
			CloseLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}

		if (UButtonSlot* const CloseButtonSlot = Cast<UButtonSlot>(CloseButton->GetContentSlot()))
		{
			if (bCreatedCloseLabel)
			{
				CloseButtonSlot->SetPadding(FMargin(12.0f, 6.0f));
				CloseButtonSlot->SetHorizontalAlignment(HAlign_Center);
				CloseButtonSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		bool bCreatedImageBox = false;
		USizeBox* const ImageBox = FindOrCreateWidget<USizeBox>(WidgetBlueprint, TEXT("ImageBox"), bCreatedImageBox);
		if (ImageBox == nullptr)
		{
			return false;
		}

		if (bCreatedImageBox)
		{
			ImageBox->SetHeightOverride(220.0f);
		}

		if (UVerticalBoxSlot* const ImageBoxSlot = Cast<UVerticalBoxSlot>(EnsurePanelChildAt(PopupContent, ImageBox, 1)))
		{
			if (bCreatedImageBox)
			{
				ImageBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
				ImageBoxSlot->SetHorizontalAlignment(HAlign_Fill);
				ImageBoxSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}

		bool bCreatedImageFrame = false;
		UBorder* const ImageFrame = FindOrCreateWidget<UBorder>(WidgetBlueprint, TEXT("ImageFrame"), bCreatedImageFrame);
		if (ImageFrame == nullptr || EnsureContent(ImageBox, ImageFrame) == nullptr)
		{
			return false;
		}

		if (bCreatedImageFrame)
		{
			ImageFrame->SetBrushColor(FLinearColor(0.14f, 0.15f, 0.18f, 1.0f));
			ImageFrame->SetPadding(FMargin(12.0f));
			ImageFrame->SetHorizontalAlignment(HAlign_Fill);
			ImageFrame->SetVerticalAlignment(VAlign_Fill);
		}

		bool bCreatedPopupImage = false;
		UImage* const PopupImage = FindOrCreateWidget<UImage>(WidgetBlueprint, TEXT("PopupImage"), bCreatedPopupImage);
		if (PopupImage == nullptr || EnsureContent(ImageFrame, PopupImage) == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, PopupImage);
		if (bCreatedPopupImage)
		{
			PopupImage->SetDesiredSizeOverride(FVector2D(504.0f, 196.0f));
		}

		bool bCreatedBodyText = false;
		UTextBlock* const BodyText = FindOrCreateWidget<UTextBlock>(WidgetBlueprint, TEXT("BodyText"), bCreatedBodyText);
		if (BodyText == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, BodyText);
		if (bCreatedBodyText)
		{
			BodyText->SetText(FText::FromString(TEXT("Primary description goes here.")));
			BodyText->SetAutoWrapText(true);
			BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.94f, 0.96f, 1.0f)));
		}

		if (UVerticalBoxSlot* const BodySlot = Cast<UVerticalBoxSlot>(EnsurePanelChildAt(PopupContent, BodyText, 2)))
		{
			if (bCreatedBodyText)
			{
				BodySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
				BodySlot->SetHorizontalAlignment(HAlign_Fill);
			}
		}

		bool bCreatedFooterText = false;
		UTextBlock* const FooterText = FindOrCreateWidget<UTextBlock>(WidgetBlueprint, TEXT("FooterText"), bCreatedFooterText);
		if (FooterText == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, FooterText);
		if (bCreatedFooterText)
		{
			FooterText->SetText(FText::FromString(TEXT("Secondary supporting text.")));
			FooterText->SetAutoWrapText(true);
			FooterText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.76f, 0.82f, 1.0f)));
		}

		if (UVerticalBoxSlot* const FooterSlot = Cast<UVerticalBoxSlot>(EnsurePanelChildAt(PopupContent, FooterText, 3)))
		{
			if (bCreatedFooterText)
			{
				FooterSlot->SetHorizontalAlignment(HAlign_Fill);
			}
		}

		return true;
	}

	bool BuildBottomButtonBarWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
	{
		check(WidgetBlueprint != nullptr);
		check(WidgetBlueprint->WidgetTree != nullptr);

		bool bCreatedRootCanvas = false;
		UCanvasPanel* const RootCanvas = EnsureRootWidget<UCanvasPanel>(WidgetBlueprint, TEXT("RootCanvas"), bCreatedRootCanvas);
		if (RootCanvas == nullptr)
		{
			return false;
		}

		bool bCreatedBarBox = false;
		USizeBox* const BarBox = FindOrCreateWidget<USizeBox>(WidgetBlueprint, TEXT("BarBox"), bCreatedBarBox);
		if (BarBox == nullptr)
		{
			return false;
		}

		if (bCreatedBarBox)
		{
			BarBox->SetWidthOverride(880.0f);
			BarBox->SetHeightOverride(96.0f);
		}

		if (UCanvasPanelSlot* const BarBoxSlot = Cast<UCanvasPanelSlot>(EnsurePanelChildAt(RootCanvas, BarBox, 0)))
		{
			if (bCreatedBarBox)
			{
				BarBoxSlot->SetAnchors(FAnchors(0.5f, 1.0f));
				BarBoxSlot->SetAlignment(FVector2D(0.5f, 1.0f));
				BarBoxSlot->SetOffsets(FMargin(0.0f, -24.0f, 880.0f, 96.0f));
			}
		}

		bool bCreatedBarFrame = false;
		UBorder* const BarFrame = FindOrCreateWidget<UBorder>(WidgetBlueprint, TEXT("BarFrame"), bCreatedBarFrame);
		if (BarFrame == nullptr || EnsureContent(BarBox, BarFrame) == nullptr)
		{
			return false;
		}

		if (bCreatedBarFrame)
		{
			BarFrame->SetPadding(FMargin(18.0f));
			BarFrame->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.11f, 0.96f));
			BarFrame->SetHorizontalAlignment(HAlign_Fill);
			BarFrame->SetVerticalAlignment(VAlign_Fill);
		}

		bool bCreatedButtonContainer = false;
		UHorizontalBox* const ButtonContainer =
			FindOrCreateWidget<UHorizontalBox>(WidgetBlueprint, TEXT("ButtonContainer"), bCreatedButtonContainer);
		if (ButtonContainer == nullptr || EnsureContent(BarFrame, ButtonContainer) == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, ButtonContainer);

		bool bCreatedTestPopupOpenButton = false;
		UButton* const TestPopupOpenButton =
			FindOrCreateWidget<UButton>(WidgetBlueprint, TEXT("TestPopupOpenButton"), bCreatedTestPopupOpenButton);
		if (TestPopupOpenButton == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, TestPopupOpenButton);
		if (bCreatedTestPopupOpenButton)
		{
			TestPopupOpenButton->SetBackgroundColor(FLinearColor(0.19f, 0.34f, 0.73f, 1.0f));
		}

		if (UHorizontalBoxSlot* const TestPopupOpenButtonSlot =
				Cast<UHorizontalBoxSlot>(EnsurePanelChildAt(ButtonContainer, TestPopupOpenButton, 0)))
		{
			if (bCreatedTestPopupOpenButton)
			{
				TestPopupOpenButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
				TestPopupOpenButtonSlot->SetHorizontalAlignment(HAlign_Left);
				TestPopupOpenButtonSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		bool bCreatedTestPopupOpenButtonLabel = false;
		UTextBlock* const TestPopupOpenButtonLabel = FindOrCreateWidget<UTextBlock>(
			WidgetBlueprint,
			TEXT("TestPopupOpenButtonLabel"),
			bCreatedTestPopupOpenButtonLabel);
		if (TestPopupOpenButtonLabel == nullptr || EnsureContent(TestPopupOpenButton, TestPopupOpenButtonLabel) == nullptr)
		{
			return false;
		}

		if (bCreatedTestPopupOpenButtonLabel)
		{
			TestPopupOpenButtonLabel->SetText(FText::FromString(TEXT("Open Test Popup")));
			TestPopupOpenButtonLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}

		if (UButtonSlot* const TestPopupOpenButtonContentSlot = Cast<UButtonSlot>(TestPopupOpenButton->GetContentSlot()))
		{
			if (bCreatedTestPopupOpenButtonLabel)
			{
				TestPopupOpenButtonContentSlot->SetPadding(FMargin(18.0f, 10.0f));
				TestPopupOpenButtonContentSlot->SetHorizontalAlignment(HAlign_Center);
				TestPopupOpenButtonContentSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		return true;
	}

	bool BuildScrollUniformGridHostWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
	{
		check(WidgetBlueprint != nullptr);
		check(WidgetBlueprint->WidgetTree != nullptr);

		bool bCreatedRootCanvas = false;
		UCanvasPanel* const RootCanvas = EnsureRootWidget<UCanvasPanel>(WidgetBlueprint, TEXT("RootCanvas"), bCreatedRootCanvas);
		if (RootCanvas == nullptr)
		{
			return false;
		}

		bool bCreatedScrollBox = false;
		UScrollBox* const GridScrollBox =
			FindOrCreateWidget<UScrollBox>(WidgetBlueprint, TEXT("GridScrollBox"), bCreatedScrollBox);
		if (GridScrollBox == nullptr)
		{
			return false;
		}

		if (UCanvasPanelSlot* const ScrollBoxSlot = Cast<UCanvasPanelSlot>(EnsurePanelChildAt(RootCanvas, GridScrollBox, 0)))
		{
			if (bCreatedScrollBox)
			{
				ScrollBoxSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				ScrollBoxSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
				ScrollBoxSlot->SetZOrder(0);
			}
		}

		bool bCreatedGridPanel = false;
		UUniformGridPanel* const GridPanel =
			FindOrCreateWidget<UUniformGridPanel>(WidgetBlueprint, TEXT("GridPanel"), bCreatedGridPanel);
		if (GridPanel == nullptr)
		{
			return false;
		}

		if (UScrollBoxSlot* const GridPanelSlot = Cast<UScrollBoxSlot>(EnsurePanelChildAt(GridScrollBox, GridPanel, 0)))
		{
			if (bCreatedGridPanel)
			{
				GridPanelSlot->SetPadding(FMargin(16.0f));
				GridPanelSlot->SetHorizontalAlignment(HAlign_Fill);
				GridPanelSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}

		if (bCreatedGridPanel)
		{
			GridPanel->SetSlotPadding(FMargin(12.0f));
			GridPanel->SetMinDesiredSlotWidth(180.0f);
			GridPanel->SetMinDesiredSlotHeight(180.0f);
		}

		return true;
	}

	bool BuildTileViewHostWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
	{
		check(WidgetBlueprint != nullptr);
		check(WidgetBlueprint->WidgetTree != nullptr);

		bool bCreatedRootCanvas = false;
		UCanvasPanel* const RootCanvas = EnsureRootWidget<UCanvasPanel>(WidgetBlueprint, TEXT("RootCanvas"), bCreatedRootCanvas);
		if (RootCanvas == nullptr)
		{
			return false;
		}

		bool bCreatedTileView = false;
		UTileView* const TileViewWidget = FindOrCreateWidget<UTileView>(WidgetBlueprint, TEXT("TileView"), bCreatedTileView);
		if (TileViewWidget == nullptr)
		{
			return false;
		}

		if (bCreatedTileView)
		{
			TileViewWidget->SetEntryWidth(180.0f);
			TileViewWidget->SetEntryHeight(180.0f);
		}

		if (UCanvasPanelSlot* const TileViewSlot = Cast<UCanvasPanelSlot>(EnsurePanelChildAt(RootCanvas, TileViewWidget, 0)))
		{
			if (bCreatedTileView)
			{
				TileViewSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				TileViewSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
				TileViewSlot->SetZOrder(0);
			}
		}

		return true;
	}

	bool BuildTileViewEntryWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
	{
		check(WidgetBlueprint != nullptr);
		check(WidgetBlueprint->WidgetTree != nullptr);

		bool bCreatedEntryBox = false;
		USizeBox* const EntryBox = EnsureRootWidget<USizeBox>(WidgetBlueprint, TEXT("EntryBox"), bCreatedEntryBox);
		if (EntryBox == nullptr)
		{
			return false;
		}

		if (bCreatedEntryBox)
		{
			EntryBox->SetMinDesiredWidth(160.0f);
			EntryBox->SetMinDesiredHeight(160.0f);
		}

		bool bCreatedEntryCard = false;
		UBorder* const EntryCard = FindOrCreateWidget<UBorder>(WidgetBlueprint, TEXT("EntryCard"), bCreatedEntryCard);
		if (EntryCard == nullptr || EnsureContent(EntryBox, EntryCard) == nullptr)
		{
			return false;
		}

		if (bCreatedEntryCard)
		{
			EntryCard->SetPadding(FMargin(14.0f));
			EntryCard->SetBrushColor(FLinearColor(0.09f, 0.10f, 0.12f, 1.0f));
			EntryCard->SetHorizontalAlignment(HAlign_Fill);
			EntryCard->SetVerticalAlignment(VAlign_Fill);
		}

		bool bCreatedEntryContent = false;
		UVerticalBox* const EntryContent =
			FindOrCreateWidget<UVerticalBox>(WidgetBlueprint, TEXT("EntryContent"), bCreatedEntryContent);
		if (EntryContent == nullptr || EnsureContent(EntryCard, EntryContent) == nullptr)
		{
			return false;
		}

		return true;
	}

#if WITH_LIVE_CODING
	FString BuildLiveCodingCompileMessage(
		const ELiveCodingCompileResult CompileResult,
		const FString& EnableErrorText) const
	{
		switch (CompileResult)
		{
		case ELiveCodingCompileResult::Success:
			return TEXT("Live Coding compile completed successfully.");
		case ELiveCodingCompileResult::NoChanges:
			return TEXT("Live Coding compile completed with no code changes.");
		case ELiveCodingCompileResult::InProgress:
			return TEXT("Live Coding compile started.");
		case ELiveCodingCompileResult::CompileStillActive:
			return TEXT("A Live Coding compile is already in progress.");
		case ELiveCodingCompileResult::NotStarted:
			if (!EnableErrorText.IsEmpty())
			{
				return EnableErrorText;
			}
			return TEXT("Live Coding could not be started for this editor session.");
		case ELiveCodingCompileResult::Failure:
			return TEXT("Live Coding compile failed.");
		case ELiveCodingCompileResult::Cancelled:
			return TEXT("Live Coding compile was cancelled.");
		default:
			return TEXT("Live Coding compile finished with an unknown result.");
		}
	}
#endif

	bool TryGetArgumentsObject(
		const TSharedRef<FJsonObject>& RequestObject,
		TSharedPtr<FJsonObject>& OutArgumentsObject,
		FString& OutError) const
	{
		OutArgumentsObject = MakeShared<FJsonObject>();
		if (!RequestObject->HasField(TEXT("arguments")))
		{
			return true;
		}

		const TSharedPtr<FJsonObject>* ArgumentsObject = nullptr;
		if (!RequestObject->TryGetObjectField(TEXT("arguments"), ArgumentsObject) || ArgumentsObject == nullptr || !ArgumentsObject->IsValid())
		{
			OutError = TEXT("arguments must be an object when provided.");
			return false;
		}

		OutArgumentsObject = *ArgumentsObject;
		return true;
	}

	bool TryGetOptionalBoolArgument(
		const TSharedPtr<FJsonObject>& ArgumentsObject,
		const FString& FieldName,
		bool& OutValue,
		FString& OutError) const
	{
		if (!ArgumentsObject.IsValid() || !ArgumentsObject->HasField(FieldName))
		{
			return true;
		}

		if (!ArgumentsObject->TryGetBoolField(FieldName, OutValue))
		{
			OutError = FString::Printf(TEXT("%s must be a boolean when provided."), *FieldName);
			return false;
		}

		return true;
	}

	bool TryGetOptionalIntArgument(
		const TSharedPtr<FJsonObject>& ArgumentsObject,
		const FString& FieldName,
		int32& OutValue,
		bool& bOutHasValue,
		FString& OutError) const
	{
		bOutHasValue = false;
		if (!ArgumentsObject.IsValid() || !ArgumentsObject->HasField(FieldName))
		{
			return true;
		}

		if (!ArgumentsObject->TryGetNumberField(FieldName, OutValue))
		{
			OutError = FString::Printf(TEXT("%s must be an integer when provided."), *FieldName);
			return false;
		}

		bOutHasValue = true;
		return true;
	}

	bool TryGetOptionalFloatArgument(
		const TSharedPtr<FJsonObject>& ArgumentsObject,
		const FString& FieldName,
		float& OutValue,
		FString& OutError) const
	{
		if (!ArgumentsObject.IsValid() || !ArgumentsObject->HasField(FieldName))
		{
			return true;
		}

		if (!ArgumentsObject->TryGetNumberField(FieldName, OutValue))
		{
			OutError = FString::Printf(TEXT("%s must be a number when provided."), *FieldName);
			return false;
		}

		return true;
	}

	bool TryGetRequiredIntArgument(
		const TSharedPtr<FJsonObject>& ArgumentsObject,
		const FString& FieldName,
		int32& OutValue,
		FString& OutError) const
	{
		if (!ArgumentsObject.IsValid() || !ArgumentsObject->HasField(FieldName))
		{
			OutError = FString::Printf(TEXT("%s is required."), *FieldName);
			return false;
		}

		if (!ArgumentsObject->TryGetNumberField(FieldName, OutValue))
		{
			OutError = FString::Printf(TEXT("%s must be an integer."), *FieldName);
			return false;
		}

		return true;
	}

	bool TryGetRequiredFloatArgument(
		const TSharedPtr<FJsonObject>& ArgumentsObject,
		const FString& FieldName,
		float& OutValue,
		FString& OutError) const
	{
		if (!ArgumentsObject.IsValid() || !ArgumentsObject->HasField(FieldName))
		{
			OutError = FString::Printf(TEXT("%s is required."), *FieldName);
			return false;
		}

		if (!ArgumentsObject->TryGetNumberField(FieldName, OutValue))
		{
			OutError = FString::Printf(TEXT("%s must be a number."), *FieldName);
			return false;
		}

		return true;
	}

	bool TryGetRequiredStringArgument(
		const TSharedPtr<FJsonObject>& ArgumentsObject,
		const FString& FieldName,
		FString& OutValue,
		FString& OutError) const
	{
		if (!ArgumentsObject.IsValid() || !ArgumentsObject->HasField(FieldName))
		{
			OutError = FString::Printf(TEXT("%s is required."), *FieldName);
			return false;
		}

		if (!ArgumentsObject->TryGetStringField(FieldName, OutValue))
		{
			OutError = FString::Printf(TEXT("%s must be a string."), *FieldName);
			return false;
		}

		OutValue = OutValue.TrimStartAndEnd();
		if (OutValue.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s must not be empty."), *FieldName);
			return false;
		}

		return true;
	}

	bool TryParseJsonBody(const TArray<uint8>& Body, TSharedPtr<FJsonObject>& OutObject, FString& OutError) const
	{
		if (Body.IsEmpty())
		{
			OutError = TEXT("Request body must not be empty.");
			return false;
		}

		const FUTF8ToTCHAR BodyAsUtf16(reinterpret_cast<const UTF8CHAR*>(Body.GetData()), Body.Num());
		const FString BodyString(BodyAsUtf16.Length(), BodyAsUtf16.Get());

		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(BodyString);
		if (!FJsonSerializer::Deserialize(JsonReader, OutObject) || !OutObject.IsValid())
		{
			OutError = TEXT("Malformed JSON request body.");
			return false;
		}

		return true;
	}

	TUniquePtr<FHttpServerResponse> CreateJsonResponse(
		const TSharedRef<FJsonObject>& JsonObject,
		EHttpServerResponseCodes ResponseCode = EHttpServerResponseCodes::Ok) const
	{
		FString SerializedBody;
		const TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&SerializedBody);
		FJsonSerializer::Serialize(JsonObject, JsonWriter);

		TUniquePtr<FHttpServerResponse> Response =
			FHttpServerResponse::Create(SerializedBody, TEXT("application/json; charset=utf-8"));
		Response->Code = ResponseCode;
		Response->Headers.FindOrAdd(TEXT("Cache-Control")).Add(TEXT("no-store"));
		return Response;
	}

	TUniquePtr<FHttpServerResponse> CreateErrorResponse(
		EHttpServerResponseCodes ResponseCode,
		const FString& ErrorCode,
		const FString& ErrorMessage,
		const FString& RequestId = FString()) const
	{
		TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
		ResponseObject->SetBoolField(TEXT("ok"), false);
		if (!RequestId.IsEmpty())
		{
			ResponseObject->SetStringField(TEXT("requestId"), RequestId);
		}

		TSharedRef<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
		ErrorObject->SetStringField(TEXT("code"), ErrorCode);
		ErrorObject->SetStringField(TEXT("message"), ErrorMessage);
		ResponseObject->SetObjectField(TEXT("error"), ErrorObject);

		return CreateJsonResponse(ResponseObject, ResponseCode);
	}

private:
	TSharedPtr<IHttpRouter> HttpRouter;
	FHttpRouteHandle HealthRouteHandle;
	FHttpRouteHandle CommandRouteHandle;
	FString PluginVersion;
};

IMPLEMENT_MODULE(FOctoMCPModule, OctoMCP)
