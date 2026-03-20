// Copyright Epic Games, Inc. All Rights Reserved.

#include "OctoMCPModule.h"

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
}

	void FOctoMCPModule::CachePluginVersion()
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

	void FOctoMCPModule::StartHttpBridge()
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

	void FOctoMCPModule::StopHttpBridge()
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

	bool FOctoMCPModule::HandleHealthRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) const
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

	bool FOctoMCPModule::HandleCommandRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) const
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

		if (Command == OctoMCP::CommandBootstrapProjectMap)
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

			FString LevelFileName = OctoMCP::DefaultBootstrapLevelFileName;
			if (!TryGetOptionalStringArgument(ArgumentsObject, TEXT("levelFileName"), LevelFileName, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			FString DirectoryPath = OctoMCP::DefaultBootstrapDirectoryPath;
			if (!TryGetOptionalStringArgument(ArgumentsObject, TEXT("directoryPath"), DirectoryPath, BodyError))
			{
				OnComplete(CreateErrorResponse(
					EHttpServerResponseCodes::BadRequest,
					TEXT("invalid_arguments"),
					BodyError,
					RequestId));
				return true;
			}

			bool bForceCreate = false;
			if (!TryGetOptionalBoolArgument(ArgumentsObject, TEXT("forceCreate"), bForceCreate, BodyError))
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
			AsyncTask(ENamedThreads::GameThread, [this, CompletionCallback, CapturedRequestId, LevelFileName, DirectoryPath, bForceCreate]()
			{
				TSharedRef<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
				ResponseObject->SetBoolField(TEXT("ok"), true);
				if (!CapturedRequestId.IsEmpty())
				{
					ResponseObject->SetStringField(TEXT("requestId"), CapturedRequestId);
				}
				ResponseObject->SetObjectField(
					TEXT("result"),
					BuildBootstrapProjectMapObject(LevelFileName, DirectoryPath, bForceCreate));

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

	TSharedRef<FJsonObject> FOctoMCPModule::BuildVersionInfoObject() const
	{
		TSharedRef<FJsonObject> ResultObject = MakeShared<FJsonObject>();
		ResultObject->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
		ResultObject->SetStringField(TEXT("buildVersion"), FString(FApp::GetBuildVersion()));
		ResultObject->SetStringField(TEXT("projectName"), FApp::GetProjectName());
		ResultObject->SetStringField(TEXT("pluginVersion"), PluginVersion);
		ResultObject->SetBoolField(TEXT("isEditor"), true);
		return ResultObject;
	}

	TSharedRef<FJsonObject> FOctoMCPModule::BuildLiveCodingCompileObject(const bool bWaitForCompletion) const
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
