#include "MCPSidebarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Styling/SlateBrush.h"

void UMCPSidebarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] NativeConstruct begin."));

	BuildSidebarLayout();

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

	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] NativeConstruct end. Buttons ready: M1=%s M2=%s M3=%s"),
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

void UMCPSidebarWidget::BuildSidebarLayout()
{
	if (WidgetTree == nullptr)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("RuntimeWidgetTree"));
		UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] WidgetTree was null. Created RuntimeWidgetTree."));
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (RootCanvas == nullptr)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = RootCanvas;
		UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Created RootCanvas."));
	}
	else
	{
		RootCanvas->ClearChildren();
		UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Reusing RootCanvas and clearing children."));
	}

	UBorder* SidebarBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SidebarBorder"));
	SidebarBorder->SetPadding(FMargin(14.0f));
	SidebarBorder->SetHorizontalAlignment(HAlign_Fill);
	SidebarBorder->SetVerticalAlignment(VAlign_Fill);

	FSlateBrush BorderBrush;
	BorderBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	BorderBrush.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	BorderBrush.OutlineSettings = FSlateBrushOutlineSettings(16.0f, FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)), 0.0f);
	SidebarBorder->SetBrush(BorderBrush);

	UCanvasPanelSlot* SidebarSlot = RootCanvas->AddChildToCanvas(SidebarBorder);
	SidebarSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	SidebarSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	SidebarSlot->SetPosition(FVector2D(16.0f, 16.0f));
	SidebarSlot->SetSize(FVector2D(240.0f, 210.0f));

	UVerticalBox* ButtonColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuButtonColumn"));
	SidebarBorder->SetContent(ButtonColumn);

	Menu1Button = CreateMenuButton(ButtonColumn, TEXT("Menu1"), FLinearColor(0.14f, 0.14f, 0.14f, 0.8f));
	Menu2Button = CreateMenuButton(ButtonColumn, TEXT("Menu2"), FLinearColor(0.14f, 0.14f, 0.14f, 0.8f));
	Menu3Button = CreateMenuButton(ButtonColumn, TEXT("Menu3"), FLinearColor(0.14f, 0.14f, 0.14f, 0.8f));
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Sidebar layout built."));
}

UButton* UMCPSidebarWidget::CreateMenuButton(UVerticalBox* ParentBox, const FString& Label, const FLinearColor& ButtonTint)
{
	if (WidgetTree == nullptr || ParentBox == nullptr)
	{
		return nullptr;
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("Button_%s"), *Label));

	FButtonStyle ButtonStyle = Button->GetStyle();
	ButtonStyle.Normal.DrawAs = ESlateBrushDrawType::RoundedBox;
	ButtonStyle.Normal.TintColor = FSlateColor(ButtonTint);
	ButtonStyle.Normal.OutlineSettings = FSlateBrushOutlineSettings(8.0f, FSlateColor(FLinearColor::Transparent), 0.0f);

	ButtonStyle.Hovered = ButtonStyle.Normal;
	ButtonStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.9f));

	ButtonStyle.Pressed = ButtonStyle.Normal;
	ButtonStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.08f, 0.08f, 0.95f));

	Button->SetStyle(ButtonStyle);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("Label_%s"), *Label));
	Text->SetText(FText::FromString(Label));
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Text->SetJustification(ETextJustify::Center);

	UButtonSlot* ButtonContentSlot = Cast<UButtonSlot>(Button->AddChild(Text));
	if (ButtonContentSlot != nullptr)
	{
		ButtonContentSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonContentSlot->SetVerticalAlignment(VAlign_Center);
		ButtonContentSlot->SetPadding(FMargin(10.0f, 8.0f));
	}

	UVerticalBoxSlot* ColumnSlot = ParentBox->AddChildToVerticalBox(Button);
	ColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	ColumnSlot->SetVerticalAlignment(VAlign_Fill);
	ColumnSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] Created menu button: %s"), *Label);
	return Button;
}

void UMCPSidebarWidget::ShowDebugMessage(const FString& Message, const FColor& Color) const
{
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, Color, Message);
	}
}
