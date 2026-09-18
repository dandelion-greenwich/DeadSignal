#include "UI/PauseMenuWidget.h"

#include "Core/DeadSignalGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ADeadSignalGameMode* GameMode = GetGameMode())
	{
		GameMode->OnPauseChanged.AddDynamic(this, &UPauseMenuWidget::HandlePauseChanged);

		// Seeded, in case something paused before this was built.
		HandlePauseChanged(GameMode->IsPaused());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Pause] Game mode is not ADeadSignalGameMode - the menu will never open."));
	}
}

void UPauseMenuWidget::NativeDestruct()
{
	if (ADeadSignalGameMode* GameMode = GetGameMode())
	{
		GameMode->OnPauseChanged.RemoveDynamic(this, &UPauseMenuWidget::HandlePauseChanged);
	}

	Super::NativeDestruct();
}

ADeadSignalGameMode* UPauseMenuWidget::GetGameMode() const
{
	return Cast<ADeadSignalGameMode>(UGameplayStatics::GetGameMode(this));
}

bool UPauseMenuWidget::IsPaused() const
{
	const ADeadSignalGameMode* GameMode = GetGameMode();
	return GameMode && GameMode->IsPaused();
}

// ---------------------------------------------------------------- Actions

void UPauseMenuWidget::Resume()
{
	if (ADeadSignalGameMode* GameMode = GetGameMode())
	{
		GameMode->SetPaused(false);
	}
}

void UPauseMenuWidget::TogglePause()
{
	if (ADeadSignalGameMode* GameMode = GetGameMode())
	{
		GameMode->TogglePause();
	}
}

void UPauseMenuWidget::RestartEncounter()
{
	if (ADeadSignalGameMode* GameMode = GetGameMode())
	{
		// Unpause first: OpenLevel while paused leaves the new level frozen
		// with no menu up to unpause it.
		GameMode->SetPaused(false);
		GameMode->RestartEncounterLevel();
	}
}

void UPauseMenuWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Only while paused, so these keys never shadow anything during play.
	if (IsPaused() && ResumeKeys.Contains(InKeyEvent.GetKey()))
	{
		Resume();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ---------------------------------------------------------------- Reacting

void UPauseMenuWidget::HandlePauseChanged(bool bIsPaused)
{
	if (bManageVisibility)
	{
		// Set before the events fire, so an entry animation in OnPaused plays
		// against a widget that is already on screen.
		SetVisibility(bIsPaused ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (bManageInputMode)
	{
		ApplyInputMode(bIsPaused);
	}

	if (bIsPaused)
	{
		OnPaused();
	}
	else
	{
		OnResumed();
	}
}

void UPauseMenuWidget::ApplyInputMode(bool bIsPaused)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (bIsPaused)
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}
