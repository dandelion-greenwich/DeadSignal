// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DeadSignalGameMode.generated.h"

class UHealthComponent;

UENUM(BlueprintType)
enum class EEncounterState : uint8
{
	Intro		UMETA(DisplayName = "Intro"),
	Fighting	UMETA(DisplayName = "Fighting"),
	Victory		UMETA(DisplayName = "Victory"),
	Defeat		UMETA(DisplayName = "Defeat")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEncounterStateChanged, EEncounterState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossRegistered, AActor*, Boss);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPauseChanged, bool, bIsPaused);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameEnded, EEncounterState, FinalState);

/**
 * Referee for the Hiramor encounter.
 *
 * Deliberately thin. It does not know what phase the boss is in, how the
 * shield works, or what a hack is - that is all boss and player behaviour.
 * It only decides when the fight starts, and which end screen you get.
 *
 * It also does not know the boss's class. It binds through UHealthComponent,
 * so any actor with one can register as the boss.
 */
UCLASS()
class BOSSFIGHTJAM_API ADeadSignalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADeadSignalGameMode();

	/** Call from the boss's BeginPlay. Binds its death to the victory path. */
	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void RegisterBoss(AActor* InBoss);

	/** Leaves Intro and lets the fight run. Called automatically unless bWaitForManualStart. */
	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void StartEncounter();

	/** Reloads the current level. */
	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void RestartEncounterLevel();

	UFUNCTION(BlueprintPure, Category = "Encounter")
	EEncounterState GetEncounterState() const { return EncounterState; }

	/**
	 * True only while the fight is live and unpaused.
	 */
	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool IsFighting() const { return EncounterState == EEncounterState::Fighting && !bIsPaused; }

	UFUNCTION(BlueprintPure, Category = "Encounter|Pause")
	bool IsPaused() const { return bIsPaused; }

	// Called from Player BP
	UFUNCTION(BlueprintCallable, Category = "Encounter|Pause")
	void TogglePause();

	UFUNCTION(BlueprintCallable, Category = "Encounter|Pause")
	void SetPaused(bool bNewPaused);

	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool IsEncounterOver() const;

	/** The registered boss, or null if it has not registered yet. Callers cast as needed. */
	UFUNCTION(BlueprintPure, Category = "Encounter")
	AActor* GetBossActor() const { return Boss; }

	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FOnEncounterStateChanged OnEncounterStateChanged;

	// Binding for the HUD
	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FOnBossRegistered OnBossRegistered;

	UPROPERTY(BlueprintAssignable, Category = "Encounter|Pause")
	FOnPauseChanged OnPauseChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FOnGameEnded OnGameEnded;

protected:
	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Encounter")
	void OnFightStarted();
	UFUNCTION(BlueprintImplementableEvent, Category = "Encounter")
	void OnVictory();
	UFUNCTION(BlueprintImplementableEvent, Category = "Encounter")
	void OnDefeat();

	/** Leave the encounter in Intro until StartEncounter is called - for an opening cinematic. */
	UPROPERTY(EditDefaultsOnly, Category = "Encounter")
	bool bWaitForManualStart = false;

	/** How far time slows when the fight ends. 1 disables the effect. */
	UPROPERTY(EditDefaultsOnly, Category = "Encounter|End Game", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float EndGameTimeDilation = 0.3f;

	/**
	 * How long the slow motion lasts, in real seconds.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Encounter|End Game", meta = (ClampMin = "0.0"))
	float EndGameSlowMoDuration = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	EEncounterState EncounterState = EEncounterState::Intro;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter|Pause")
	bool bIsPaused = false;

private:
	UFUNCTION()
	void HandlePlayerDeath();
	UFUNCTION()
	void HandleBossDeath();
	void SetEncounterState(EEncounterState NewState);

	/** Shared victory and defeat path: close the fight, slow time, then show the screen. */
	void BeginEndGameSequence(EEncounterState FinalState);

	/** Restores time and announces the end screen. */
	void FinishEndGameSequence();

	/** Binds to an actor's health component if it has one. Returns the component, or null. */
	UHealthComponent* BindDeathHandler(AActor* Actor, bool bIsPlayer);

	UPROPERTY()
	TObjectPtr<AActor> Boss;
	UPROPERTY()
	TObjectPtr<UHealthComponent> BossHealth;
	UPROPERTY()
	TObjectPtr<UHealthComponent> PlayerHealth;
	FTimerHandle EndScreenTimer;
};
