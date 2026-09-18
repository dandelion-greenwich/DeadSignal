#include "Audio/MusicDirectorComponent.h"

#include "Combat/BossCharacter.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UMusicDirectorComponent::UMusicDirectorComponent()
{
	// Nothing to tick. Every fade here is run by the audio engine, which keeps
	// running while the game is paused - a hand-rolled fade in TickComponent
	// would stall halfway through a pause and never finish.
	PrimaryComponentTick.bCanEverTick = false;
}

void UMusicDirectorComponent::BeginPlay()
{
	Super::BeginPlay();

	GameMode = Cast<ADeadSignalGameMode>(UGameplayStatics::GetGameMode(this));
	if (!GameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Music] Game mode is not ADeadSignalGameMode - no music will play."));
		return;
	}

	GameMode->OnEncounterStateChanged.AddDynamic(this, &UMusicDirectorComponent::HandleEncounterStateChanged);
	GameMode->OnGameEnded.AddDynamic(this, &UMusicDirectorComponent::HandleGameEnded);
	GameMode->OnPauseChanged.AddDynamic(this, &UMusicDirectorComponent::HandlePauseChanged);
	GameMode->OnBossRegistered.AddDynamic(this, &UMusicDirectorComponent::HandleBossRegistered);

	// The boss may have registered already or may arrive later - its BeginPlay
	// and ours have no guaranteed order. Cover both.
	BindToBoss(GameMode->GetBossActor());

	// Seeded rather than waiting for a broadcast
	HandleEncounterStateChanged(GameMode->GetEncounterState());
}

void UMusicDirectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GameMode)
	{
		GameMode->OnEncounterStateChanged.RemoveDynamic(this, &UMusicDirectorComponent::HandleEncounterStateChanged);
		GameMode->OnGameEnded.RemoveDynamic(this, &UMusicDirectorComponent::HandleGameEnded);
		GameMode->OnPauseChanged.RemoveDynamic(this, &UMusicDirectorComponent::HandlePauseChanged);
		GameMode->OnBossRegistered.RemoveDynamic(this, &UMusicDirectorComponent::HandleBossRegistered);
	}

	if (Boss)
	{
		Boss->OnPhaseChanged.RemoveDynamic(this, &UMusicDirectorComponent::HandlePhaseChanged);
		Boss->OnShieldStateChanged.RemoveDynamic(this, &UMusicDirectorComponent::HandleShieldStateChanged);
	}

	if (ActiveMusic)
	{
		// Stopped rather than faded: on a restart there is nothing left to fade
		// against, and the old track would bleed over the reloaded level.
		ActiveMusic->Stop();
		ActiveMusic = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------ Playback

void UMusicDirectorComponent::PlayTrack(USoundBase* NewTrack, float FadeDuration)
{
	if (!NewTrack)
	{
		return;
	}

	// Re-entering a state must not restart the track from the top.
	if (ActiveMusic && ActiveMusic->Sound == NewTrack)
	{
		return;
	}

	if (ActiveMusic)
	{
		// Handed to its own fade and forgotten. bAutoDestroy tears the component
		// down once the fade ends, so overlapping fades need no bookkeeping and
		// there is no second handle here to go stale.
		ActiveMusic->FadeOut(FadeDuration, 0.f);
		ActiveMusic = nullptr;
	}

	// Created rather than spawned: SpawnSound2D starts the track immediately at
	// full volume, which FadeIn would then have to restart. This begins silent.
	ActiveMusic = UGameplayStatics::CreateSound2D(this, NewTrack, 1.f, 1.f, 0.f,
		/*ConcurrencySettings=*/nullptr, /*bPersistAcrossLevelTransition=*/false, /*bAutoDestroy=*/true);

	if (!ActiveMusic)
	{
		return;
	}

	// CreateSound2D already sets this - restated because the music depends on
	// it. SetGamePaused stops every active sound that is not flagged as UI, so
	// without it the pause menu would sit in silence. Music is the one sound
	// that should ignore the pause; every effect should freeze with the game.
	ActiveMusic->bIsUISound = true;

	// FadeIn starts playback, so there is no Play call. The fader carries the
	// volume rather than the component, which makes ducking later one
	// AdjustVolume against the same scale.
	ActiveMusic->FadeIn(FadeDuration, MusicVolume);
}

void UMusicDirectorComponent::StopMusic(float FadeDuration)
{
	if (ActiveMusic)
	{
		ActiveMusic->FadeOut(FadeDuration, 0.f);
		ActiveMusic = nullptr;
	}
}

USoundBase* UMusicDirectorComponent::GetCurrentTrack() const
{
	if (ActiveMusic)
	{
		return ActiveMusic->Sound;
	}

	return nullptr;
}

// ------------------------------------------------------------------ Reacting

USoundBase* UMusicDirectorComponent::ResolveTrack() const
{
	if (!GameMode || GameMode->GetEncounterState() == EEncounterState::Intro)
	{
		return IntroMusic;
	}

	if (ShieldDownMusic && Boss && Boss->GetShieldState() == EShieldState::Down)
	{
		return ShieldDownMusic;
	}

	// Read off the boss rather than remembered. Phase 1 no longer broadcasts,
	// so asking is the only way to know where the fight opens.
	return FindPhaseMusic(Boss ? Boss->GetCurrentPhase() : EBossPhase::Phase1);
}

void UMusicDirectorComponent::RefreshTrack()
{
	if (!GameMode || GameMode->IsEncounterOver())
	{
		return;
	}

	PlayTrack(ResolveTrack(), CrossfadeDuration);
}

void UMusicDirectorComponent::HandleEncounterStateChanged(EEncounterState NewState)
{
	switch (NewState)
	{
	case EEncounterState::Intro:
	case EEncounterState::Fighting:
		RefreshTrack();
		break;

	case EEncounterState::Victory:
	case EEncounterState::Defeat:
		if (ActiveMusic)
		{
			ActiveMusic->SetPitchMultiplier(EndGamePitchMultiplier);
		}
		StopMusic(EndGameFadeDuration);
		break;
	}
}

void UMusicDirectorComponent::HandleGameEnded(EEncounterState FinalState)
{
	PlayTrack(FinalState == EEncounterState::Victory ? VictoryMusic : DefeatMusic, CrossfadeDuration);
}

void UMusicDirectorComponent::HandlePauseChanged(bool bIsPaused)
{
	if (!ActiveMusic)
	{
		return;
	}

	// AdjustVolume rather than a lerp of our own: component tick is frozen
	// while paused, so a hand-rolled duck would stop partway down and the
	// music would never come back up.
	ActiveMusic->AdjustVolume(PauseDuckDuration,
		bIsPaused ? MusicVolume * PausedVolumeMultiplier : MusicVolume);
}

// ---------------------------------------------------------------------- Boss

void UMusicDirectorComponent::HandleBossRegistered(AActor* BossActor)
{
	BindToBoss(BossActor);
}

void UMusicDirectorComponent::BindToBoss(AActor* BossActor)
{
	// Guarded so the delegate firing after we already bound is harmless.
	if (Boss || !BossActor)
	{
		return;
	}

	Boss = Cast<ABossCharacter>(BossActor);
	if (!Boss)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Music] Registered boss %s is not an ABossCharacter - phase music will never change."),
			*BossActor->GetName());
		return;
	}

	Boss->OnPhaseChanged.AddDynamic(this, &UMusicDirectorComponent::HandlePhaseChanged);
	Boss->OnShieldStateChanged.AddDynamic(this, &UMusicDirectorComponent::HandleShieldStateChanged);

	// Seeded, because phase 1 no longer broadcasts and the shield starts up
	// silently. Without this the fight would open on whatever was playing.
	RefreshTrack();
}

void UMusicDirectorComponent::HandlePhaseChanged(EBossPhase NewPhase)
{
	RefreshTrack();
}

void UMusicDirectorComponent::HandleShieldStateChanged(EShieldState NewState)
{
	RefreshTrack();
}

USoundBase* UMusicDirectorComponent::FindPhaseMusic(EBossPhase Phase) const
{
	if (const TObjectPtr<USoundBase>* Found = PhaseMusic.Find(Phase))
	{
		return *Found;
	}

	return nullptr;
}
