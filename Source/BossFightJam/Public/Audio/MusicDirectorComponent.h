#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DeadSignalGameMode.h"
#include "Core/DeadSignalTypes.h"
#include "MusicDirectorComponent.generated.h"

class ABossCharacter;
class UAudioComponent;
class USoundBase;

/**
 * Owns the music layer for the encounter.
 *
 * Add it to the game mode blueprint. Nothing calls it - it binds to the
 * encounter and boss delegates and cross-fades in response, so the game mode
 * stays ignorant of phases and the boss stays ignorant of music.
 *
 * Music is the only audio that needs an owner like this: it is global, it is
 * stateful (a cross-fade has to know what is already playing) and it outlives
 * the actors that trigger it. One-shots - gunshots, impacts, telegraphs - have
 * an obvious owner already and belong on those blueprint hooks directly.
 */
UCLASS(ClassGroup = (DeadSignal), meta = (BlueprintSpawnableComponent))
class BOSSFIGHTJAM_API UMusicDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMusicDirectorComponent();

	/** Cross-fade to a track. Null keeps whatever is already playing. */
	UFUNCTION(BlueprintCallable, Category = "Music")
	void PlayTrack(USoundBase* NewTrack, float FadeDuration);

	/** Fade the music out and leave silence. */
	UFUNCTION(BlueprintCallable, Category = "Music")
	void StopMusic(float FadeDuration);

	UFUNCTION(BlueprintPure, Category = "Music")
	USoundBase* GetCurrentTrack() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------------------------------------------------------------- Tracks

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Tracks")
	TObjectPtr<USoundBase> IntroMusic;

	/**
	 * One track per phase. A phase with no entry holds the previous track
	 * rather than cutting to silence, so shipping with fewer tracks than
	 * phases is a safe state rather than a bug.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Tracks")
	TMap<EBossPhase, TObjectPtr<USoundBase>> PhaseMusic;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Tracks")
	TObjectPtr<USoundBase> ShieldDownMusic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Tracks")
	TObjectPtr<USoundBase> VictoryMusic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Tracks")
	TObjectPtr<USoundBase> DefeatMusic;

	// ---------------------------------------------------------------- Mixing

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Mixing", meta = (ClampMin = "0.0"))
	float MusicVolume = 1.f;

	UPROPERTY(EditAnywhere, Category = "Music|Mixing", meta = (ClampMin = "0.0"))
	float CrossfadeDuration = 1.5f;

	/** How far the music drops while paused. 1 disables ducking. */
	UPROPERTY(EditAnywhere, Category = "Music|Mixing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PausedVolumeMultiplier = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Music|Mixing", meta = (ClampMin = "0.0"))
	float PauseDuckDuration = 0.25f;

	// -------------------------------------------------------------- End game

	/**
	 * Pitch the fight music drops to once the fight is decided, to match the
	 * game mode's slow motion. 1 disables the effect.
	 */
	UPROPERTY(EditAnywhere, Category = "Music|End Game", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float EndGamePitchMultiplier = 0.3f;

	/** How long the fight music takes to die once the fight is decided. */
	UPROPERTY(EditAnywhere, Category = "Music|End Game", meta = (ClampMin = "0.0"))
	float EndGameFadeDuration = 1.f;

private:
	UFUNCTION()
	void HandleEncounterStateChanged(EEncounterState NewState);

	UFUNCTION()
	void HandleGameEnded(EEncounterState FinalState);

	UFUNCTION()
	void HandlePauseChanged(bool bIsPaused);

	UFUNCTION()
	void HandleBossRegistered(AActor* BossActor);

	UFUNCTION()
	void HandlePhaseChanged(EBossPhase NewPhase);

	UFUNCTION()
	void HandleShieldStateChanged(EShieldState NewState);

	void BindToBoss(AActor* BossActor);

	/**
	 * The track the current world state calls for.
	 */
	USoundBase* ResolveTrack() const;

	/** Play whatever ResolveTrack now calls for. */
	void RefreshTrack();

	/** The track for a phase, or null if that phase has no entry. */
	USoundBase* FindPhaseMusic(EBossPhase Phase) const;

	/** The one track playing. Outgoing tracks are left to their own fade. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveMusic;

	UPROPERTY(Transient)
	TObjectPtr<ADeadSignalGameMode> GameMode;

	UPROPERTY(Transient)
	TObjectPtr<ABossCharacter> Boss;
};
