// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/DeadSignalGameMode.h"

#include "Combat/HealthComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ADeadSignalGameMode::ADeadSignalGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADeadSignalGameMode::BeginPlay()
{
	Super::BeginPlay();

	// The boss registers itself, so it may already have arrived by now.
	if (!bWaitForManualStart)
	{
		StartEncounter();
	}
}

void ADeadSignalGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	// Called once the pawn exists, which BeginPlay cannot guarantee.
	if (NewPlayer)
	{
		PlayerHealth = BindDeathHandler(NewPlayer->GetPawn(), /*bIsPlayer=*/true);
	}
}

void ADeadSignalGameMode::RegisterBoss(AActor* InBoss)
{
	if (!InBoss || Boss == InBoss)
	{
		return;
	}

	Boss = InBoss;
	BossHealth = BindDeathHandler(InBoss, /*bIsPlayer=*/false);

	OnBossRegistered.Broadcast(InBoss);
}

UHealthComponent* ADeadSignalGameMode::BindDeathHandler(AActor* Actor, bool bIsPlayer)
{
	if (!Actor)
	{
		return nullptr;
	}

	UHealthComponent* Health = Actor->FindComponentByClass<UHealthComponent>();
	if (!Health)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no UHealthComponent - its death will never be detected."),
			*Actor->GetName());
		return nullptr;
	}

	if (bIsPlayer)
	{
		Health->OnDeath.AddDynamic(this, &ADeadSignalGameMode::HandlePlayerDeath);
	}
	else
	{
		Health->OnDeath.AddDynamic(this, &ADeadSignalGameMode::HandleBossDeath);
	}

	return Health;
}

void ADeadSignalGameMode::StartEncounter()
{
	if (EncounterState != EEncounterState::Intro)
	{
		return;
	}

	SetEncounterState(EEncounterState::Fighting);
	OnFightStarted();
}

void ADeadSignalGameMode::HandlePlayerDeath()
{
	if (IsEncounterOver())
	{
		return;
	}

	BeginEndGameSequence(EEncounterState::Defeat);
}

void ADeadSignalGameMode::HandleBossDeath()
{
	if (IsEncounterOver())
	{
		return;
	}

	BeginEndGameSequence(EEncounterState::Victory);
}

void ADeadSignalGameMode::BeginEndGameSequence(EEncounterState FinalState)
{
	SetEncounterState(FinalState);

	UGameplayStatics::SetGlobalTimeDilation(this, EndGameTimeDilation);
	
	const float ScaledDelay = FMath::Max(EndGameSlowMoDuration * EndGameTimeDilation, 0.001f);

	GetWorldTimerManager().SetTimer(EndScreenTimer, this,
		&ADeadSignalGameMode::FinishEndGameSequence, ScaledDelay, false);
}

void ADeadSignalGameMode::FinishEndGameSequence()
{
	// Restored before the screen appears, or every UI animation on it would
	// play at a third speed.
	UGameplayStatics::SetGlobalTimeDilation(this, 0.f);

	OnGameEnded.Broadcast(EncounterState);

	if (EncounterState == EEncounterState::Victory)
	{
		OnVictory();
	}
	else
	{
		OnDefeat();
	}
}

void ADeadSignalGameMode::SetEncounterState(EEncounterState NewState)
{
	if (EncounterState == NewState)
	{
		return;
	}

	EncounterState = NewState;
	OnEncounterStateChanged.Broadcast(NewState);
}

bool ADeadSignalGameMode::IsEncounterOver() const
{
	return EncounterState == EEncounterState::Victory || EncounterState == EEncounterState::Defeat;
}

void ADeadSignalGameMode::RestartEncounterLevel()
{
	GetWorldTimerManager().ClearTimer(EndScreenTimer);

	// The new level would otherwise inherit whatever the end sequence or the
	// pause menu left behind.
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	UGameplayStatics::SetGamePaused(this, false);

	const FName CurrentLevel(*UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString=*/true));
	UGameplayStatics::OpenLevel(this, CurrentLevel);
}

// ---------------------------------------------------------------- Pause

void ADeadSignalGameMode::TogglePause()
{
	SetPaused(!bIsPaused);
}

void ADeadSignalGameMode::SetPaused(bool bNewPaused)
{
	if (bIsPaused == bNewPaused)
	{
		return;
	}

	// Refused once the fight is over: pausing a victory screen does nothing
	// useful and would leave the player stuck behind a menu with no fight.
	if (bNewPaused && IsEncounterOver())
	{
		return;
	}

	bIsPaused = bNewPaused;

	// Freezes tick and every timer, so hack cooldowns, attack sequences and
	// projectile lifespans all hold rather than draining while paused.
	UGameplayStatics::SetGamePaused(this, bIsPaused);

	OnPauseChanged.Broadcast(bIsPaused);
}
