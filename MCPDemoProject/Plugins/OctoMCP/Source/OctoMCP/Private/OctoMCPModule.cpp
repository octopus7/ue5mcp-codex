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
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/EditorAssetSubsystem.h"
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
	const TCHAR* const CommandCreateWidgetBlueprint = TEXT("create_widget_blueprint");
	const TCHAR* const CommandScaffoldWidgetBlueprint = TEXT("scaffold_widget_blueprint");
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

		ResetWidgetBlueprintTree(WidgetBlueprint);

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

		Widget->bIsVariable = true;
		WidgetBlueprint->OnVariableAdded(Widget->GetFName());
	}

	bool BuildPopupWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
	{
		check(WidgetBlueprint != nullptr);
		check(WidgetBlueprint->WidgetTree != nullptr);

		UWidgetTree* const WidgetTree = WidgetBlueprint->WidgetTree;

		UCanvasPanel* const RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		if (RootCanvas == nullptr)
		{
			return false;
		}

		WidgetTree->RootWidget = RootCanvas;

		UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
		Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.58f));

		if (UCanvasPanelSlot* const BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop))
		{
			BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			BackdropSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
			BackdropSlot->SetZOrder(0);
		}

		UBorder* const PopupCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PopupCard"));
		PopupCard->SetPadding(FMargin(28.0f));
		PopupCard->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.11f, 1.0f));
		PopupCard->SetHorizontalAlignment(HAlign_Fill);
		PopupCard->SetVerticalAlignment(VAlign_Fill);

		if (UCanvasPanelSlot* const PopupCardSlot = RootCanvas->AddChildToCanvas(PopupCard))
		{
			PopupCardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PopupCardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PopupCardSlot->SetOffsets(FMargin(0.0f, 0.0f, 560.0f, 640.0f));
			PopupCardSlot->SetZOrder(1);
		}

		UVerticalBox* const PopupContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PopupContent"));
		PopupCard->AddChild(PopupContent);

		UHorizontalBox* const HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
		if (UVerticalBoxSlot* const HeaderSlot = PopupContent->AddChildToVerticalBox(HeaderRow))
		{
			HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
			HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
			HeaderSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* const TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
		TitleText->SetText(FText::FromString(TEXT("Popup Title")));
		TitleText->SetAutoWrapText(true);
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		RegisterBindableWidget(WidgetBlueprint, TitleText);

		if (UHorizontalBoxSlot* const TitleSlot = HeaderRow->AddChildToHorizontalBox(TitleText))
		{
			FSlateChildSize FillSize(ESlateSizeRule::Fill);
			FillSize.Value = 1.0f;
			TitleSlot->SetSize(FillSize);
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			TitleSlot->SetHorizontalAlignment(HAlign_Left);
			TitleSlot->SetVerticalAlignment(VAlign_Center);
		}

		UButton* const CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
		CloseButton->SetBackgroundColor(FLinearColor(0.16f, 0.17f, 0.20f, 1.0f));
		RegisterBindableWidget(WidgetBlueprint, CloseButton);

		if (UHorizontalBoxSlot* const CloseSlot = HeaderRow->AddChildToHorizontalBox(CloseButton))
		{
			CloseSlot->SetHorizontalAlignment(HAlign_Right);
			CloseSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* const CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLabel"));
		CloseLabel->SetText(FText::FromString(TEXT("Close")));
		CloseLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		if (UButtonSlot* const CloseButtonSlot = Cast<UButtonSlot>(CloseButton->AddChild(CloseLabel)))
		{
			CloseButtonSlot->SetPadding(FMargin(12.0f, 6.0f));
			CloseButtonSlot->SetHorizontalAlignment(HAlign_Center);
			CloseButtonSlot->SetVerticalAlignment(VAlign_Center);
		}

		USizeBox* const ImageBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ImageBox"));
		ImageBox->SetHeightOverride(220.0f);
		if (UVerticalBoxSlot* const ImageBoxSlot = PopupContent->AddChildToVerticalBox(ImageBox))
		{
			ImageBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
			ImageBoxSlot->SetHorizontalAlignment(HAlign_Fill);
			ImageBoxSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UBorder* const ImageFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ImageFrame"));
		ImageFrame->SetBrushColor(FLinearColor(0.14f, 0.15f, 0.18f, 1.0f));
		ImageFrame->SetPadding(FMargin(12.0f));
		ImageFrame->SetHorizontalAlignment(HAlign_Fill);
		ImageFrame->SetVerticalAlignment(VAlign_Fill);
		ImageBox->AddChild(ImageFrame);

		UImage* const PopupImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PopupImage"));
		PopupImage->SetDesiredSizeOverride(FVector2D(504.0f, 196.0f));
		RegisterBindableWidget(WidgetBlueprint, PopupImage);
		ImageFrame->AddChild(PopupImage);

		UTextBlock* const BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BodyText"));
		BodyText->SetText(FText::FromString(TEXT("Primary description goes here.")));
		BodyText->SetAutoWrapText(true);
		BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.94f, 0.96f, 1.0f)));
		RegisterBindableWidget(WidgetBlueprint, BodyText);

		if (UVerticalBoxSlot* const BodySlot = PopupContent->AddChildToVerticalBox(BodyText))
		{
			BodySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
			BodySlot->SetHorizontalAlignment(HAlign_Fill);
		}

		UTextBlock* const FooterText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FooterText"));
		FooterText->SetText(FText::FromString(TEXT("Secondary supporting text.")));
		FooterText->SetAutoWrapText(true);
		FooterText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.76f, 0.82f, 1.0f)));
		RegisterBindableWidget(WidgetBlueprint, FooterText);

		if (UVerticalBoxSlot* const FooterSlot = PopupContent->AddChildToVerticalBox(FooterText))
		{
			FooterSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		return true;
	}

	bool BuildBottomButtonBarWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
	{
		check(WidgetBlueprint != nullptr);
		check(WidgetBlueprint->WidgetTree != nullptr);

		UWidgetTree* const WidgetTree = WidgetBlueprint->WidgetTree;

		UCanvasPanel* const RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		if (RootCanvas == nullptr)
		{
			return false;
		}

		WidgetTree->RootWidget = RootCanvas;

		USizeBox* const BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
		BarBox->SetWidthOverride(880.0f);
		BarBox->SetHeightOverride(96.0f);

		if (UCanvasPanelSlot* const BarBoxSlot = RootCanvas->AddChildToCanvas(BarBox))
		{
			BarBoxSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			BarBoxSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			BarBoxSlot->SetOffsets(FMargin(0.0f, -24.0f, 880.0f, 96.0f));
		}

		UBorder* const BarFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BarFrame"));
		BarFrame->SetPadding(FMargin(18.0f));
		BarFrame->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.11f, 0.96f));
		BarFrame->SetHorizontalAlignment(HAlign_Fill);
		BarFrame->SetVerticalAlignment(VAlign_Fill);
		BarBox->AddChild(BarFrame);

		UHorizontalBox* const ButtonContainer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonContainer"));
		RegisterBindableWidget(WidgetBlueprint, ButtonContainer);
		BarFrame->AddChild(ButtonContainer);

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
