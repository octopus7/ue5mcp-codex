#include "MCPSidebarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BorderSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Styling/SlateBrush.h"

namespace
{
template <typename TWidgetType>
TWidgetType* FindNamedWidget(UWidgetTree* WidgetTree, const TCHAR* ManagedName, const TCHAR* LegacyName = nullptr)
{
	if (WidgetTree == nullptr)
	{
		return nullptr;
	}

	if (ManagedName != nullptr)
	{
		if (TWidgetType* Found = Cast<TWidgetType>(WidgetTree->FindWidget(FName(ManagedName))))
		{
			return Found;
		}
	}

	if (LegacyName != nullptr)
	{
		return Cast<TWidgetType>(WidgetTree->FindWidget(FName(LegacyName)));
	}

	return nullptr;
}

void ApplyButtonVisual(UButton* Button)
{
	if (Button == nullptr)
	{
		return;
	}

	FButtonStyle ButtonStyle = Button->GetStyle();
	ButtonStyle.Normal.DrawAs = ESlateBrushDrawType::RoundedBox;
	ButtonStyle.Normal.TintColor = FSlateColor(FLinearColor(0.14f, 0.14f, 0.14f, 0.8f));
	ButtonStyle.Normal.OutlineSettings = FSlateBrushOutlineSettings(8.0f, FSlateColor(FLinearColor::Transparent), 0.0f);

	ButtonStyle.Hovered = ButtonStyle.Normal;
	ButtonStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.9f));

	ButtonStyle.Pressed = ButtonStyle.Normal;
	ButtonStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.08f, 0.08f, 0.95f));

	Button->SetStyle(ButtonStyle);

	if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button->Slot))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonSlot->SetVerticalAlignment(VAlign_Center);
		ButtonSlot->SetPadding(FMargin(10.0f, 8.0f));
	}
}
}

void UMCPSidebarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] NativeConstruct begin."));

	ResolveWidgets();
	ApplyVisualStyle();
	ApplyLabels();

	if (Menu1Button != nullptr)
	{
		Menu1Button->OnClicked.RemoveDynamic(this, &UMCPSidebarWidget::HandleMenu1Clicked);
		Menu1Button->OnClicked.AddDynamic(this, &UMCPSidebarWidget::HandleMenu1Clicked);
	}

	if (Menu2Button != nullptr)
	{
		Menu2Button->OnClicked.RemoveDynamic(this, &UMCPSidebarWidget::HandleMenu2Clicked);
		Menu2Button->OnClicked.AddDynamic(this, &UMCPSidebarWidget::HandleMenu2Clicked);
	}

	if (Menu3Button != nullptr)
	{
		Menu3Button->OnClicked.RemoveDynamic(this, &UMCPSidebarWidget::HandleMenu3Clicked);
		Menu3Button->OnClicked.AddDynamic(this, &UMCPSidebarWidget::HandleMenu3Clicked);
	}

	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] NativeConstruct end. Panel=%s Buttons: M1=%s M2=%s M3=%s"),
		SidebarPanel != nullptr ? TEXT("true") : TEXT("false"),
		Menu1Button != nullptr ? TEXT("true") : TEXT("false"),
		Menu2Button != nullptr ? TEXT("true") : TEXT("false"),
		Menu3Button != nullptr ? TEXT("true") : TEXT("false"));
}

void UMCPSidebarWidget::HandleMenu1Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Menu1 clicked."));
	ShowDebugMessage(TEXT("Menu1 clicked"), FColor::Green);
}

void UMCPSidebarWidget::HandleMenu2Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Menu2 clicked."));
	ShowDebugMessage(TEXT("Menu2 clicked"), FColor::Yellow);
}

void UMCPSidebarWidget::HandleMenu3Clicked()
{
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Menu3 clicked."));
	ShowDebugMessage(TEXT("Menu3 clicked"), FColor::Cyan);
}

void UMCPSidebarWidget::ResolveWidgets()
{
	SidebarPanel = FindNamedWidget<UBorder>(WidgetTree, TEXT("MCP_SidebarPanel"), TEXT("SidebarPanel"));
	Menu1Button = FindNamedWidget<UButton>(WidgetTree, TEXT("MCP_Menu1Button"), TEXT("Menu1Button"));
	Menu2Button = FindNamedWidget<UButton>(WidgetTree, TEXT("MCP_Menu2Button"), TEXT("Menu2Button"));
	Menu3Button = FindNamedWidget<UButton>(WidgetTree, TEXT("MCP_Menu3Button"), TEXT("Menu3Button"));
	Menu1Label = FindNamedWidget<UTextBlock>(WidgetTree, TEXT("MCP_Menu1Label"), TEXT("Menu1Label"));
	Menu2Label = FindNamedWidget<UTextBlock>(WidgetTree, TEXT("MCP_Menu2Label"), TEXT("Menu2Label"));
	Menu3Label = FindNamedWidget<UTextBlock>(WidgetTree, TEXT("MCP_Menu3Label"), TEXT("Menu3Label"));

	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] ResolveWidgets: panel=%s btn1=%s btn2=%s btn3=%s"),
		SidebarPanel != nullptr ? TEXT("ok") : TEXT("missing"),
		Menu1Button != nullptr ? TEXT("ok") : TEXT("missing"),
		Menu2Button != nullptr ? TEXT("ok") : TEXT("missing"),
		Menu3Button != nullptr ? TEXT("ok") : TEXT("missing"));
}

void UMCPSidebarWidget::ApplyVisualStyle()
{
	if (SidebarPanel != nullptr)
	{
		SidebarPanel->SetPadding(FMargin(14.0f));
		SidebarPanel->SetHorizontalAlignment(HAlign_Fill);
		SidebarPanel->SetVerticalAlignment(VAlign_Fill);

		FSlateBrush BorderBrush;
		BorderBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		BorderBrush.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		BorderBrush.OutlineSettings = FSlateBrushOutlineSettings(16.0f, FSlateColor(FLinearColor::Transparent), 0.0f);
		SidebarPanel->SetBrush(BorderBrush);
	}

	if (UVerticalBox* ButtonColumn = FindNamedWidget<UVerticalBox>(WidgetTree, TEXT("MCP_MenuButtonColumn"), TEXT("MenuButtonColumn")))
	{
		if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(ButtonColumn->Slot))
		{
			BorderSlot->SetPadding(FMargin(0.0f));
			BorderSlot->SetHorizontalAlignment(HAlign_Fill);
			BorderSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	ApplyButtonVisual(Menu1Button);
	ApplyButtonVisual(Menu2Button);
	ApplyButtonVisual(Menu3Button);

	if (UVerticalBoxSlot* Menu1VBoxSlot = Menu1Button != nullptr ? Cast<UVerticalBoxSlot>(Menu1Button->Slot) : nullptr)
	{
		Menu1VBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		Menu1VBoxSlot->SetVerticalAlignment(VAlign_Fill);
		Menu1VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	if (UVerticalBoxSlot* Menu2VBoxSlot = Menu2Button != nullptr ? Cast<UVerticalBoxSlot>(Menu2Button->Slot) : nullptr)
	{
		Menu2VBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		Menu2VBoxSlot->SetVerticalAlignment(VAlign_Fill);
		Menu2VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	if (UVerticalBoxSlot* Menu3VBoxSlot = Menu3Button != nullptr ? Cast<UVerticalBoxSlot>(Menu3Button->Slot) : nullptr)
	{
		Menu3VBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		Menu3VBoxSlot->SetVerticalAlignment(VAlign_Fill);
		Menu3VBoxSlot->SetPadding(FMargin(0.0f));
	}
}

void UMCPSidebarWidget::ApplyLabels()
{
	if (Menu1Label != nullptr)
	{
		Menu1Label->SetText(FText::FromString(TEXT("Menu1")));
		Menu1Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Menu1Label->SetJustification(ETextJustify::Center);
	}

	if (Menu2Label != nullptr)
	{
		Menu2Label->SetText(FText::FromString(TEXT("Menu2")));
		Menu2Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Menu2Label->SetJustification(ETextJustify::Center);
	}

	if (Menu3Label != nullptr)
	{
		Menu3Label->SetText(FText::FromString(TEXT("Menu3")));
		Menu3Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Menu3Label->SetJustification(ETextJustify::Center);
	}
}

void UMCPSidebarWidget::ShowDebugMessage(const FString& Message, const FColor& Color) const
{
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, Color, Message);
	}
}
