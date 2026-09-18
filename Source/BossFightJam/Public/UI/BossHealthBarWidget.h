#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/DeadSignalTypes.h"
#include "BossHealthBarWidget.generated.h"

class ABossCharacter;
class UHealthComponent;
class UProgressBar;
class UTextBlock;

/**
 * Base class for WBP_BossBar.
 *
 * Unlike the player's bar this cannot read the owning pawn - it finds the boss
 * through the game mode. The boss registers itself in its own BeginPlay, which
 * may run after the HUD is built, so this also listens for it arriving rather
 * than assuming it is already there.
 */
UCLASS(Abstract)
class BOSSFIGHTJAM_API UBossHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** False until the boss registers. Hide the bar on this. */
	UFUNCTION(BlueprintPure, Category = "Boss Bar")
	bool HasBoss() const { return Boss != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Boss Bar")
	ABossCharacter* GetBoss() const { return Boss; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * The bar. Required - name a Progress Bar "HealthBar" in the Blueprint and
	 * C++ fills it on every change. Its style stays in Blueprint.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Boss Bar")
	TObjectPtr<UProgressBar> HealthBar;

	/** Optional. C++ writes "64 / 100" into it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Boss Bar")
	TObjectPtr<UTextBlock> HealthText;

	/** Optional. C++ writes "Phase 2" into it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Boss Bar")
	TObjectPtr<UTextBlock> PhaseText;

	/** The boss registered and everything is now bound. Show the bar here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Bar")
	void OnBossFound();

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Bar")
	void OnHealthUpdated(float CurrentHealth, float MaxHealth, float Percent);

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Bar")
	void OnPhaseChanged(EBossPhase NewPhase);

	/**
	 * The single most important thing the player needs to read at a glance -
	 * whether damage is landing. Worth making loud.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Bar")
	void OnShieldStateChanged(EShieldState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Bar")
	void OnBossDied();

private:
	/** Binds the boss's health and state delegates. Safe to call twice. */
	void BindToBoss(AActor* BossActor);

	UFUNCTION()
	void HandleBossRegistered(AActor* BossActor);

	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth);

	UFUNCTION()
	void HandlePhaseChanged(EBossPhase NewPhase);

	UFUNCTION()
	void HandleShieldStateChanged(EShieldState NewState);

	UFUNCTION()
	void HandleDeath();

	UPROPERTY()
	TObjectPtr<ABossCharacter> Boss;

	UPROPERTY()
	TObjectPtr<UHealthComponent> BossHealth;
};
