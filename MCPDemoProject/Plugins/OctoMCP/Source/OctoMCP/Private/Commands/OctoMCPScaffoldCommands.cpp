// Copyright Epic Games, Inc. All Rights Reserved.

#include "OctoMCPModule.h"

	TSharedRef<FJsonObject> FOctoMCPModule::BuildScaffoldWidgetBlueprintObject(
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

	FScaffoldWidgetBlueprintResult FOctoMCPModule::ScaffoldWidgetBlueprintAsset(
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
		else if (Result.ScaffoldType == TEXT("item_tile_popup"))
		{
			bBuiltScaffold = BuildItemTilePopupWidgetScaffold(WidgetBlueprint);
		}
		else if (Result.ScaffoldType == TEXT("item_tile_entry"))
		{
			bBuiltScaffold = BuildItemTileEntryWidgetScaffold(WidgetBlueprint);
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

	void FOctoMCPModule::ResetWidgetBlueprintTree(UWidgetBlueprint* WidgetBlueprint) const
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

	void FOctoMCPModule::RegisterBindableWidget(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget) const
	{
		check(WidgetBlueprint != nullptr);
		check(Widget != nullptr);

		if (!Widget->bIsVariable)
		{
			Widget->bIsVariable = true;
			WidgetBlueprint->OnVariableAdded(Widget->GetFName());
		}
	}

	bool FOctoMCPModule::RenameWidget(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget, const TCHAR* NewWidgetName) const
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

	UPanelSlot* FOctoMCPModule::EnsureContent(UContentWidget* ParentWidget, UWidget* ChildWidget) const
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

	UPanelSlot* FOctoMCPModule::EnsurePanelChildAt(UPanelWidget* ParentWidget, UWidget* ChildWidget, const int32 DesiredIndex) const
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

	bool FOctoMCPModule::BuildPopupWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
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

	bool FOctoMCPModule::BuildBottomButtonBarWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
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

		bool bCreatedTestTilePopupOpenButton = false;
		UButton* const TestTilePopupOpenButton =
			FindOrCreateWidget<UButton>(WidgetBlueprint, TEXT("TestTilePopupOpenButton"), bCreatedTestTilePopupOpenButton);
		if (TestTilePopupOpenButton == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, TestTilePopupOpenButton);
		if (bCreatedTestTilePopupOpenButton)
		{
			TestTilePopupOpenButton->SetBackgroundColor(FLinearColor(0.92f, 0.54f, 0.18f, 1.0f));
		}

		if (UHorizontalBoxSlot* const TestTilePopupOpenButtonSlot =
				Cast<UHorizontalBoxSlot>(EnsurePanelChildAt(ButtonContainer, TestTilePopupOpenButton, 1)))
		{
			if (bCreatedTestTilePopupOpenButton)
			{
				TestTilePopupOpenButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
				TestTilePopupOpenButtonSlot->SetHorizontalAlignment(HAlign_Left);
				TestTilePopupOpenButtonSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		bool bCreatedTestTilePopupOpenButtonLabel = false;
		UTextBlock* const TestTilePopupOpenButtonLabel = FindOrCreateWidget<UTextBlock>(
			WidgetBlueprint,
			TEXT("TestTilePopupOpenButtonLabel"),
			bCreatedTestTilePopupOpenButtonLabel);
		if (TestTilePopupOpenButtonLabel == nullptr || EnsureContent(TestTilePopupOpenButton, TestTilePopupOpenButtonLabel) == nullptr)
		{
			return false;
		}

		if (bCreatedTestTilePopupOpenButtonLabel)
		{
			TestTilePopupOpenButtonLabel->SetText(FText::FromString(TEXT("Open Tile Popup")));
			TestTilePopupOpenButtonLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}

		if (UButtonSlot* const TestTilePopupOpenButtonContentSlot = Cast<UButtonSlot>(TestTilePopupOpenButton->GetContentSlot()))
		{
			if (bCreatedTestTilePopupOpenButtonLabel)
			{
				TestTilePopupOpenButtonContentSlot->SetPadding(FMargin(18.0f, 10.0f));
				TestTilePopupOpenButtonContentSlot->SetHorizontalAlignment(HAlign_Center);
				TestTilePopupOpenButtonContentSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		return true;
	}

	bool FOctoMCPModule::BuildScrollUniformGridHostWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
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

	bool FOctoMCPModule::BuildTileViewHostWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
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

	bool FOctoMCPModule::BuildTileViewEntryWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
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

	bool FOctoMCPModule::BuildItemTilePopupWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
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
			Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.42f));
		}

		if (UCanvasPanelSlot* const BackdropSlot = Cast<UCanvasPanelSlot>(EnsurePanelChildAt(RootCanvas, Backdrop, 0)))
		{
			if (bCreatedBackdrop)
			{
				BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				BackdropSlot->SetOffsets(FMargin(0.0f));
				BackdropSlot->SetZOrder(0);
			}
		}

		bool bCreatedPopupCard = false;
		UBackgroundBlur* const PopupCard =
			FindOrCreateWidget<UBackgroundBlur>(WidgetBlueprint, TEXT("PopupCard"), bCreatedPopupCard);
		if (PopupCard == nullptr)
		{
			return false;
		}

		if (bCreatedPopupCard)
		{
			PopupCard->SetPadding(FMargin(28.0f));
			PopupCard->SetHorizontalAlignment(HAlign_Fill);
			PopupCard->SetVerticalAlignment(VAlign_Fill);
			PopupCard->SetBlurStrength(26.0f);
			PopupCard->SetApplyAlphaToBlur(false);
			PopupCard->SetOverrideAutoRadiusCalculation(true);
			PopupCard->SetBlurRadius(24);
			PopupCard->SetCornerRadius(FVector4(20.0f, 20.0f, 20.0f, 20.0f));
		}

		if (UCanvasPanelSlot* const PopupCardSlot = Cast<UCanvasPanelSlot>(EnsurePanelChildAt(RootCanvas, PopupCard, 1)))
		{
			if (bCreatedPopupCard)
			{
				PopupCardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
				PopupCardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				PopupCardSlot->SetOffsets(FMargin(0.0f, 0.0f, 920.0f, 720.0f));
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

		bool bCreatedTitleText = false;
		UTextBlock* const TitleText = FindOrCreateWidget<UTextBlock>(WidgetBlueprint, TEXT("TitleText"), bCreatedTitleText);
		if (TitleText == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, TitleText);
		if (bCreatedTitleText)
		{
			TitleText->SetText(FText::FromString(TEXT("Tile Item List Test")));
			TitleText->SetAutoWrapText(true);
			TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}

		if (UHorizontalBoxSlot* const TitleTextSlot = Cast<UHorizontalBoxSlot>(EnsurePanelChildAt(HeaderRow, TitleText, 0)))
		{
			if (bCreatedTitleText)
			{
				FSlateChildSize FillSize(ESlateSizeRule::Fill);
				FillSize.Value = 1.0f;
				TitleTextSlot->SetSize(FillSize);
				TitleTextSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
				TitleTextSlot->SetHorizontalAlignment(HAlign_Left);
				TitleTextSlot->SetVerticalAlignment(VAlign_Center);
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
			CloseButton->SetBackgroundColor(FLinearColor(0.15f, 0.16f, 0.19f, 1.0f));
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

		bool bCreatedTileViewBox = false;
		USizeBox* const TileViewBox = FindOrCreateWidget<USizeBox>(WidgetBlueprint, TEXT("TileViewBox"), bCreatedTileViewBox);
		if (TileViewBox == nullptr)
		{
			return false;
		}

		if (bCreatedTileViewBox)
		{
			TileViewBox->SetHeightOverride(520.0f);
		}

		if (UVerticalBoxSlot* const TileViewBoxSlot = Cast<UVerticalBoxSlot>(EnsurePanelChildAt(PopupContent, TileViewBox, 1)))
		{
			if (bCreatedTileViewBox)
			{
				TileViewBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
				TileViewBoxSlot->SetHorizontalAlignment(HAlign_Fill);
				TileViewBoxSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}

		bool bCreatedTileViewFrame = false;
		UBorder* const TileViewFrame = FindOrCreateWidget<UBorder>(WidgetBlueprint, TEXT("TileViewFrame"), bCreatedTileViewFrame);
		if (TileViewFrame == nullptr || EnsureContent(TileViewBox, TileViewFrame) == nullptr)
		{
			return false;
		}

		if (bCreatedTileViewFrame)
		{
			TileViewFrame->SetPadding(FMargin(14.0f));
			TileViewFrame->SetBrushColor(FLinearColor(0.08f, 0.10f, 0.14f, 0.86f));
			TileViewFrame->SetHorizontalAlignment(HAlign_Fill);
			TileViewFrame->SetVerticalAlignment(VAlign_Fill);
		}

		bool bCreatedItemTileView = false;
		UTileView* const ItemTileView = FindOrCreateWidget<UTileView>(WidgetBlueprint, TEXT("ItemTileView"), bCreatedItemTileView);
		if (ItemTileView == nullptr || EnsureContent(TileViewFrame, ItemTileView) == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, ItemTileView);
		if (bCreatedItemTileView)
		{
			ItemTileView->SetEntryWidth(164.0f);
			ItemTileView->SetEntryHeight(212.0f);
		}

		bool bCreatedActionRow = false;
		UHorizontalBox* const ActionRow = FindOrCreateWidget<UHorizontalBox>(WidgetBlueprint, TEXT("ActionRow"), bCreatedActionRow);
		if (ActionRow == nullptr)
		{
			return false;
		}

		if (UVerticalBoxSlot* const ActionRowSlot = Cast<UVerticalBoxSlot>(EnsurePanelChildAt(PopupContent, ActionRow, 2)))
		{
			if (bCreatedActionRow)
			{
				ActionRowSlot->SetHorizontalAlignment(HAlign_Left);
				ActionRowSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		bool bCreatedClearButton = false;
		UButton* const ClearButton = FindOrCreateWidget<UButton>(WidgetBlueprint, TEXT("ClearButton"), bCreatedClearButton);
		if (ClearButton == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, ClearButton);
		if (bCreatedClearButton)
		{
			ClearButton->SetBackgroundColor(FLinearColor(0.27f, 0.31f, 0.36f, 1.0f));
		}

		if (UHorizontalBoxSlot* const ClearButtonSlot = Cast<UHorizontalBoxSlot>(EnsurePanelChildAt(ActionRow, ClearButton, 0)))
		{
			if (bCreatedClearButton)
			{
				ClearButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
				ClearButtonSlot->SetHorizontalAlignment(HAlign_Left);
				ClearButtonSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		bool bCreatedClearLabel = false;
		UTextBlock* const ClearLabel = FindOrCreateWidget<UTextBlock>(WidgetBlueprint, TEXT("ClearLabel"), bCreatedClearLabel);
		if (ClearLabel == nullptr || EnsureContent(ClearButton, ClearLabel) == nullptr)
		{
			return false;
		}

		if (bCreatedClearLabel)
		{
			ClearLabel->SetText(FText::FromString(TEXT("Clear")));
			ClearLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}

		if (UButtonSlot* const ClearButtonContentSlot = Cast<UButtonSlot>(ClearButton->GetContentSlot()))
		{
			if (bCreatedClearLabel)
			{
				ClearButtonContentSlot->SetPadding(FMargin(18.0f, 10.0f));
				ClearButtonContentSlot->SetHorizontalAlignment(HAlign_Center);
				ClearButtonContentSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		bool bCreatedRandomizeButton = false;
		UButton* const RandomizeButton =
			FindOrCreateWidget<UButton>(WidgetBlueprint, TEXT("RandomizeButton"), bCreatedRandomizeButton);
		if (RandomizeButton == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, RandomizeButton);
		if (bCreatedRandomizeButton)
		{
			RandomizeButton->SetBackgroundColor(FLinearColor(0.83f, 0.47f, 0.18f, 1.0f));
		}

		if (UHorizontalBoxSlot* const RandomizeButtonSlot =
				Cast<UHorizontalBoxSlot>(EnsurePanelChildAt(ActionRow, RandomizeButton, 1)))
		{
			if (bCreatedRandomizeButton)
			{
				RandomizeButtonSlot->SetHorizontalAlignment(HAlign_Left);
				RandomizeButtonSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		bool bCreatedRandomizeLabel = false;
		UTextBlock* const RandomizeLabel =
			FindOrCreateWidget<UTextBlock>(WidgetBlueprint, TEXT("RandomizeLabel"), bCreatedRandomizeLabel);
		if (RandomizeLabel == nullptr || EnsureContent(RandomizeButton, RandomizeLabel) == nullptr)
		{
			return false;
		}

		if (bCreatedRandomizeLabel)
		{
			RandomizeLabel->SetText(FText::FromString(TEXT("Randomize")));
			RandomizeLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}

		if (UButtonSlot* const RandomizeButtonContentSlot = Cast<UButtonSlot>(RandomizeButton->GetContentSlot()))
		{
			if (bCreatedRandomizeLabel)
			{
				RandomizeButtonContentSlot->SetPadding(FMargin(18.0f, 10.0f));
				RandomizeButtonContentSlot->SetHorizontalAlignment(HAlign_Center);
				RandomizeButtonContentSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		return true;
	}

	bool FOctoMCPModule::BuildItemTileEntryWidgetScaffold(UWidgetBlueprint* WidgetBlueprint) const
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
			EntryBox->SetWidthOverride(164.0f);
			EntryBox->SetHeightOverride(212.0f);
		}

		bool bCreatedEntryCard = false;
		UBorder* const EntryCard = FindOrCreateWidget<UBorder>(WidgetBlueprint, TEXT("EntryCard"), bCreatedEntryCard);
		if (EntryCard == nullptr || EnsureContent(EntryBox, EntryCard) == nullptr)
		{
			return false;
		}

		if (bCreatedEntryCard)
		{
			EntryCard->SetPadding(FMargin(12.0f));
			EntryCard->SetBrushColor(FLinearColor(0.10f, 0.11f, 0.14f, 1.0f));
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

		bool bCreatedItemImageBox = false;
		USizeBox* const ItemImageBox =
			FindOrCreateWidget<USizeBox>(WidgetBlueprint, TEXT("ItemImageBox"), bCreatedItemImageBox);
		if (ItemImageBox == nullptr)
		{
			return false;
		}

		if (bCreatedItemImageBox)
		{
			ItemImageBox->SetHeightOverride(136.0f);
		}

		if (UVerticalBoxSlot* const ItemImageBoxSlot =
				Cast<UVerticalBoxSlot>(EnsurePanelChildAt(EntryContent, ItemImageBox, 0)))
		{
			if (bCreatedItemImageBox)
			{
				ItemImageBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
				ItemImageBoxSlot->SetHorizontalAlignment(HAlign_Fill);
				ItemImageBoxSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}

		bool bCreatedItemImageFrame = false;
		UBorder* const ItemImageFrame =
			FindOrCreateWidget<UBorder>(WidgetBlueprint, TEXT("ItemImageFrame"), bCreatedItemImageFrame);
		if (ItemImageFrame == nullptr || EnsureContent(ItemImageBox, ItemImageFrame) == nullptr)
		{
			return false;
		}

		if (bCreatedItemImageFrame)
		{
			ItemImageFrame->SetPadding(FMargin(10.0f));
			ItemImageFrame->SetBrushColor(FLinearColor(0.15f, 0.17f, 0.22f, 1.0f));
			ItemImageFrame->SetHorizontalAlignment(HAlign_Fill);
			ItemImageFrame->SetVerticalAlignment(VAlign_Fill);
		}

		bool bCreatedItemImage = false;
		UImage* const ItemImage = FindOrCreateWidget<UImage>(WidgetBlueprint, TEXT("ItemImage"), bCreatedItemImage);
		if (ItemImage == nullptr || EnsureContent(ItemImageFrame, ItemImage) == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, ItemImage);
		if (bCreatedItemImage)
		{
			ItemImage->SetDesiredSizeOverride(FVector2D(112.0f, 112.0f));
		}

		bool bCreatedQuantityText = false;
		UTextBlock* const QuantityText =
			FindOrCreateWidget<UTextBlock>(WidgetBlueprint, TEXT("QuantityText"), bCreatedQuantityText);
		if (QuantityText == nullptr)
		{
			return false;
		}

		RegisterBindableWidget(WidgetBlueprint, QuantityText);
		if (bCreatedQuantityText)
		{
			QuantityText->SetText(FText::FromString(TEXT("x0")));
			QuantityText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}

		if (UVerticalBoxSlot* const QuantityTextSlot =
				Cast<UVerticalBoxSlot>(EnsurePanelChildAt(EntryContent, QuantityText, 1)))
		{
			if (bCreatedQuantityText)
			{
				QuantityTextSlot->SetHorizontalAlignment(HAlign_Left);
				QuantityTextSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		return true;
	}

#if WITH_LIVE_CODING
	FString FOctoMCPModule::BuildLiveCodingCompileMessage(
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
