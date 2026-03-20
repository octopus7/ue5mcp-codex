// Copyright Epic Games, Inc. All Rights Reserved.

#include "OctoMCPModule.h"

	bool FOctoMCPModule::TryGetArgumentsObject(
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

	bool FOctoMCPModule::TryGetOptionalBoolArgument(
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

	bool FOctoMCPModule::TryGetOptionalStringArgument(
		const TSharedPtr<FJsonObject>& ArgumentsObject,
		const FString& FieldName,
		FString& OutValue,
		FString& OutError) const
	{
		if (!ArgumentsObject.IsValid() || !ArgumentsObject->HasField(FieldName))
		{
			return true;
		}

		if (!ArgumentsObject->TryGetStringField(FieldName, OutValue))
		{
			OutError = FString::Printf(TEXT("%s must be a string when provided."), *FieldName);
			return false;
		}

		OutValue = OutValue.TrimStartAndEnd();
		if (OutValue.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s must not be empty when provided."), *FieldName);
			return false;
		}

		return true;
	}

	bool FOctoMCPModule::TryGetOptionalIntArgument(
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

	bool FOctoMCPModule::TryGetOptionalFloatArgument(
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

	bool FOctoMCPModule::TryGetRequiredIntArgument(
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

	bool FOctoMCPModule::TryGetRequiredFloatArgument(
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

	bool FOctoMCPModule::TryGetRequiredStringArgument(
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

	bool FOctoMCPModule::TryParseJsonBody(const TArray<uint8>& Body, TSharedPtr<FJsonObject>& OutObject, FString& OutError) const
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

	TUniquePtr<FHttpServerResponse> FOctoMCPModule::CreateJsonResponse(
		const TSharedRef<FJsonObject>& JsonObject,
		EHttpServerResponseCodes ResponseCode) const
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

	TUniquePtr<FHttpServerResponse> FOctoMCPModule::CreateErrorResponse(
		EHttpServerResponseCodes ResponseCode,
		const FString& ErrorCode,
		const FString& ErrorMessage,
		const FString& RequestId) const
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
