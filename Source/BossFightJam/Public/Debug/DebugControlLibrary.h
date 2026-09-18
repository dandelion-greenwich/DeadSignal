#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/DeadSignalTypes.h"
#include "Core/DeadSignalGameMode.h"
#include "DebugControlLibrary.generated.h"

class UHealthComponent;
class UHackComponent;
class UWeaponComponent;
class ABossCharacter;

/** One hack's state, flattened for display. */
USTRUCT(BlueprintType)
struct FHackDebugInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	FText DisplayName;

	/** Available / Active / Cooling. */
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	FString State;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float RemainingSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	TArray<int32> KeySequence;
};

/**
 * Everything the debug panel can read or do.
 *
 * Statics rather than a subsystem because the Slate window that drives this is
 * not owned by any world - it outlives PIE sessions, so every call resolves the
 * current world rather than holding one.
 */
UCLASS()
class BOSSFIGHTJAM_API UDebugControlLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** False when nothing is playing, so readouts can show "-" instead of zeros. */
	UFUNCTION(BlueprintPure, Category = "Debug")
	static bool IsGameRunning();

	// ------------------------------------------------------------ Player

	UFUNCTION(BlueprintPure, Category = "Debug|Player")
	static float GetPlayerHealth();

	UFUNCTION(BlueprintPure, Category = "Debug|Player")
	static float GetPlayerMaxHealth();

	UFUNCTION(BlueprintPure, Category = "Debug|Player")
	static bool IsPlayerInvulnerable();

	UFUNCTION(BlueprintPure, Category = "Debug|Player")
	static bool IsPlayerInHitImmunity();

	UFUNCTION(BlueprintPure, Category = "Debug|Player")
	static int32 GetPlayerAmmo();

	UFUNCTION(BlueprintPure, Category = "Debug|Player")
	static int32 GetPlayerMagazineSize();

	/** Routed through ApplyDamage, so invulnerability and i-frames still apply. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Player")
	static void DamagePlayer(float Amount);

	/** Bypasses invulnerability deliberately - for testing the death path. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Player")
	static void KillPlayer();

	/** Amount <= 0 heals to full. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Player")
	static void HealPlayer(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Debug|Player")
	static void SetPlayerGodMode(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Debug|Player")
	static void RefillAmmo();

	// ------------------------------------------------------------ Boss

	UFUNCTION(BlueprintPure, Category = "Debug|Boss")
	static bool HasBoss();

	UFUNCTION(BlueprintPure, Category = "Debug|Boss")
	static float GetBossHealth();

	UFUNCTION(BlueprintPure, Category = "Debug|Boss")
	static float GetBossMaxHealth();

	UFUNCTION(BlueprintPure, Category = "Debug|Boss")
	static EBossPhase GetBossPhase();

	UFUNCTION(BlueprintPure, Category = "Debug|Boss")
	static EShieldState GetBossShieldState();

	UFUNCTION(BlueprintPure, Category = "Debug|Boss")
	static bool IsBossStunned();

	/** Routed through ApplyDamage, so the shield multiplier still applies. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Boss")
	static void DamageBoss(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Debug|Boss")
	static void KillBoss();

	UFUNCTION(BlueprintCallable, Category = "Debug|Boss")
	static void SetBossHealth(float Value);

	/** Jumps directly, bypassing the one-band-at-a-time clamp. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Boss")
	static void SetBossPhase(EBossPhase Phase);

	UFUNCTION(BlueprintCallable, Category = "Debug|Boss")
	static void DropBossShield(float Seconds);

	UFUNCTION(BlueprintCallable, Category = "Debug|Boss")
	static void StunBoss(float Seconds);

	UFUNCTION(BlueprintPure, Category = "Debug|Boss")
	static bool IsBossTransitioning();

	/** Seconds left in the damage window, 0 when the shield is up. */
	UFUNCTION(BlueprintPure, Category = "Debug|Boss")
	static float GetBossShieldRemaining();

	// ------------------------------------------------------------ Boss attacks

	UFUNCTION(BlueprintPure, Category = "Debug|Attacks")
	static bool IsBossAttacking();

	/** 1-based, for display. 0 when no sequence is running. */
	UFUNCTION(BlueprintPure, Category = "Debug|Attacks")
	static int32 GetBossStepNumber();

	UFUNCTION(BlueprintPure, Category = "Debug|Attacks")
	static int32 GetBossSequenceLength();

	UFUNCTION(BlueprintPure, Category = "Debug|Attacks")
	static FString GetBossCurrentAttackName();

	// ------------------------------------------------------------ Projectile pool

	UFUNCTION(BlueprintPure, Category = "Debug|Pool")
	static int32 GetPoolFreeCount();

	/** Projectiles in flight right now. */
	UFUNCTION(BlueprintPure, Category = "Debug|Pool")
	static int32 GetPoolActiveCount();

	/** Every projectile ever created. Should plateau after the prewarm. */
	UFUNCTION(BlueprintPure, Category = "Debug|Pool")
	static int32 GetPoolTotalCount();

	// ------------------------------------------------------------ Hacks

	UFUNCTION(BlueprintPure, Category = "Debug|Hacks")
	static int32 GetHackCount();

	UFUNCTION(BlueprintPure, Category = "Debug|Hacks")
	static FHackDebugInfo GetHackInfo(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Debug|Hacks")
	static void ResetHackCooldowns();

	// ------------------------------------------------------------ Encounter

	UFUNCTION(BlueprintPure, Category = "Debug|Encounter")
	static EEncounterState GetEncounterState();

	UFUNCTION(BlueprintCallable, Category = "Debug|Encounter")
	static void RestartEncounter();

	UFUNCTION(BlueprintCallable, Category = "Debug|Encounter")
	static void StartEncounter();

	/** One-line summary of everything, for logging. */
	UFUNCTION(BlueprintPure, Category = "Debug")
	static FString GetStatusSummary();

private:
	/**
	 * Prefers a live PIE world, falling back to any game world. The panel is
	 * not tied to a world, so this must run per call rather than being cached.
	 */
	static UWorld* ResolveWorld();

	static APawn* GetPlayerPawn();
	static UHealthComponent* GetPlayerHealthComponent();
	static UHackComponent* GetHackComponent();
	static UWeaponComponent* GetWeaponComponent();
	static ABossCharacter* GetBoss();
	static ADeadSignalGameMode* GetGameMode();
};
