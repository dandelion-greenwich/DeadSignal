#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamaged, float, AmountApplied, AActor*, DamageInstigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

/**
 * Health, damage and death for anything that can be hurt.
 *
 * Used by both the player and the boss. Everything is delegate-driven so UI
 * binds once and never polls on Tick.
 *
 * Damage reduction lives here via DamageMultiplier: the boss sets it low while
 * its shield holds and back to 1.0 when hack 1 drops it, so the shield rule
 * has exactly one home.
 */
UCLASS(ClassGroup = (DeadSignal), meta = (BlueprintSpawnableComponent))
class BOSSFIGHTJAM_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	/** Applies damage after invulnerability and DamageMultiplier. Returns damage actually dealt. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float ApplyDamage(float Amount, AActor* DamageInstigator = nullptr);

	/** Restores health up to MaxHealth. Does nothing once dead. Returns health actually restored. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float Heal(float Amount);

	/** Sets health directly, clamped. Use for resets, not for combat. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetHealth(float NewHealth);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ResetHealth();

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	/** 0-1, for health bars. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetInvulnerable(bool bNewInvulnerable) { bIsInvulnerable = bNewInvulnerable; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsInvulnerable() const { return bIsInvulnerable; }

	/**
	 * Brief grace period after a hit lands. Entirely separate from
	 * bIsInvulnerable so hack 3 can be activated, expire or toggle at any point
	 * during an i-frame window without either clobbering the other.
	 */
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsInHitImmunity() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetRemainingHitImmunity() const;

	/** 1.0 = full damage. The boss drops this while its shield is up. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetDamageMultiplier(float NewMultiplier);

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetDamageMultiplier() const { return DamageMultiplier; }

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeath OnDeath;

	UPROPERTY(EditAnywhere, Category = "Health|Audio")
	TObjectPtr<USoundBase> HealSound;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** Ignores all damage while true. Hack 3 sets this on the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	bool bIsInvulnerable = false;

	/** Scales incoming damage. The boss's shield uses this rather than a hard gate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.f;

	/**
	 * Seconds of immunity granted after any hit lands. 0 disables it.
	 *
	 * Set to 0.5 on the player so a dense bullet pattern cannot delete them in
	 * three frames. Left at 0 on the boss - i-frames there would silently cap
	 * the player's DPS during the very window the fight is built around.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "0.0"))
	float HitImmunityDuration = 0.f;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;

	FTimerHandle HitImmunityTimer;
};
