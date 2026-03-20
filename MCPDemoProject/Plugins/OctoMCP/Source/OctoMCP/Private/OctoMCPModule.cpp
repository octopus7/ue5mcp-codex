// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/Async.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
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
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "GameMapsSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
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
#include "Misc/PackageName.h"
#include "PluginDescriptor.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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
	const TCHAR* const CommandScaffoldWidgetBlueprint = TEXT("scaffold_widget_blueprint");
	const TCHAR* const CommandSetBlueprintClassProperty = TEXT("set_blueprint_class_property");
	const TCHAR* const CommandSetGlobalDefaultGameMode = TEXT("set_global_default_game_mode");
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
		UBorder* const PopupCard = FindOrCreateWidget<UBorder>(WidgetBlueprint, TEXT("PopupCard"), bCreatedPopupCard);
		if (PopupCard == nullptr)
		{
			return false;
		}

		if (bCreatedPopupCard)
		{
			PopupCard->SetPadding(FMargin(28.0f));
			PopupCard->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.11f, 1.0f));
			PopupCard->SetHorizontalAlignment(HAlign_Fill);
			PopupCard->SetVerticalAlignment(VAlign_Fill);
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

		UTextBlock* HeaderText = FindWidgetByName<UTextBlock>(WidgetBlueprint, TEXT("HeaderText"));
		if (HeaderText == nullptr)
		{
			if (UTextBlock* const LegacyHeaderText = FindWidgetByName<UTextBlock>(WidgetBlueprint, TEXT("TitleText")))
			{
				if (LegacyHeaderText->GetParent() == HeaderRow)
				{
					if (!RenameWidget(WidgetBlueprint, LegacyHeaderText, TEXT("HeaderText")))
					{
						return false;
					}
					HeaderText = FindWidgetByName<UTextBlock>(WidgetBlueprint, TEXT("HeaderText"));
				}
			}
		}

		bool bCreatedHeaderText = false;
		if (HeaderText == nullptr)
		{
			HeaderText = FindOrCreateWidget<UTextBlock>(WidgetBlueprint, TEXT("HeaderText"), bCreatedHeaderText);
		}

		if (HeaderText == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, HeaderText);

		if (bCreatedHeaderText)
		{
			HeaderText->SetText(FText::FromString(TEXT("Popup Header")));
			HeaderText->SetAutoWrapText(true);
			HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}

		if (UHorizontalBoxSlot* const HeaderTextSlot = Cast<UHorizontalBoxSlot>(EnsurePanelChildAt(HeaderRow, HeaderText, 0)))
		{
			if (bCreatedHeaderText)
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

		bool bCreatedTitleText = false;
		UTextBlock* const TitleText = FindOrCreateWidget<UTextBlock>(WidgetBlueprint, TEXT("TitleText"), bCreatedTitleText);
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

		if (UVerticalBoxSlot* const TitleTextSlot = Cast<UVerticalBoxSlot>(EnsurePanelChildAt(PopupContent, TitleText, 2)))
		{
			if (bCreatedTitleText)
			{
				TitleTextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
				TitleTextSlot->SetHorizontalAlignment(HAlign_Fill);
			}
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

		if (UVerticalBoxSlot* const BodySlot = Cast<UVerticalBoxSlot>(EnsurePanelChildAt(PopupContent, BodyText, 3)))
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

		if (UVerticalBoxSlot* const FooterSlot = Cast<UVerticalBoxSlot>(EnsurePanelChildAt(PopupContent, FooterText, 4)))
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
