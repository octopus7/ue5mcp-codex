// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPPopupWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"

void UMCPPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CloseButton != nullptr)
	{
		CloseButton->OnClicked.RemoveAll(this);
		CloseButton->OnClicked.AddDynamic(this, &UMCPPopupWidget::HandleCloseButtonClicked);
	}
}

void UMCPPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	StartOpenElasticScaleAnimation();
}

void UMCPPopupWidget::NativeDestruct()
{
	StopOpenElasticScaleAnimation();

	Super::NativeDestruct();
}

void UMCPPopupWidget::HandleCloseButtonClicked()
{
	RemoveFromParent();
}

void UMCPPopupWidget::StartOpenElasticScaleAnimation()
{
	StopOpenElasticScaleAnimation();

	OpenElasticAnimatedWidget = ResolveOpenElasticScaleTargetWidget();
	if (OpenElasticAnimatedWidget == nullptr)
	{
		return;
	}

	OpenElasticAnimatedWidget->SetRenderTransformPivot(OpenElasticPivot);

	if (!bPlayOpenElasticScaleAnimation)
	{
		ApplyOpenElasticScale(1.0f);
		return;
	}

	OpenElasticElapsedTime = 0.0f;
	OpenElasticLastTimestamp = -1.0;
	bOpenElasticScaleAnimationActive = true;

	ApplyOpenElasticScale(OpenElasticStartScale);

	if (UWorld* const World = GetWorld())
	{
		OpenElasticScaleTimerHandle =
			World->GetTimerManager().SetTimerForNextTick(this, &UMCPPopupWidget::HandleOpenElasticScaleAnimationTick);
	}
}

void UMCPPopupWidget::StopOpenElasticScaleAnimation()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OpenElasticScaleTimerHandle);
	}

	bOpenElasticScaleAnimationActive = false;
	OpenElasticElapsedTime = 0.0f;
	OpenElasticLastTimestamp = -1.0;

	if (OpenElasticAnimatedWidget != nullptr)
	{
		ApplyOpenElasticScale(1.0f);
	}

	OpenElasticAnimatedWidget = nullptr;
}

void UMCPPopupWidget::HandleOpenElasticScaleAnimationTick()
{
	if (!bOpenElasticScaleAnimationActive || OpenElasticAnimatedWidget == nullptr)
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		StopOpenElasticScaleAnimation();
		return;
	}

	const double CurrentTimestamp = FPlatformTime::Seconds();
	if (OpenElasticLastTimestamp < 0.0)
	{
		OpenElasticLastTimestamp = CurrentTimestamp;
	}

	const float DeltaTime = static_cast<float>(CurrentTimestamp - OpenElasticLastTimestamp);
	OpenElasticLastTimestamp = CurrentTimestamp;
	OpenElasticElapsedTime += FMath::Max(DeltaTime, 0.0f);

	const float Duration = FMath::Max(OpenElasticScaleDuration, 0.05f);
	const float Alpha = FMath::Clamp(OpenElasticElapsedTime / Duration, 0.0f, 1.0f);
	const float EasedAlpha = EvaluateElasticOutAlpha(Alpha);
	const float CurrentScale = FMath::Lerp(OpenElasticStartScale, 1.0f, EasedAlpha);

	ApplyOpenElasticScale(CurrentScale);

	if (Alpha >= 1.0f)
	{
		StopOpenElasticScaleAnimation();
		return;
	}

	OpenElasticScaleTimerHandle =
		World->GetTimerManager().SetTimerForNextTick(this, &UMCPPopupWidget::HandleOpenElasticScaleAnimationTick);
}

UWidget* UMCPPopupWidget::ResolveOpenElasticScaleTargetWidget() const
{
	if (!OpenElasticTargetWidgetName.IsNone())
	{
		if (UWidget* const NamedWidget = GetWidgetFromName(OpenElasticTargetWidgetName))
		{
			return NamedWidget;
		}
	}

	return GetRootWidget();
}

float UMCPPopupWidget::EvaluateElasticOutAlpha(const float Alpha) const
{
	if (Alpha <= 0.0f)
	{
		return 0.0f;
	}

	if (Alpha >= 1.0f)
	{
		return 1.0f;
	}

	const float OscillationCount = FMath::Max(OpenElasticOscillationCount, 0.5f);
	return 1.0f - FMath::Pow(2.0f, -10.0f * Alpha) * FMath::Cos(2.0f * PI * OscillationCount * Alpha);
}

void UMCPPopupWidget::ApplyOpenElasticScale(const float Scale) const
{
	if (OpenElasticAnimatedWidget != nullptr)
	{
		const float ClampedScale = FMath::Max(Scale, 0.01f);
		OpenElasticAnimatedWidget->SetRenderScale(FVector2D(ClampedScale, ClampedScale));
	}
}
