#pragma once

#include "CoreMinimal.h"
#include "DeadSignalTypes.generated.h"

/**
 * Shared vocabulary for DEAD//SIGNAL.
 *
 * This header is a leaf: it includes nothing of ours, so player, boss and UI
 * can all include it without creating a circular dependency. Anything both
 * sides of the fight need to name belongs here.
 */

UENUM(BlueprintType)
enum class EBossPhase : uint8
{
	Phase1	UMETA(DisplayName = "Phase 1"),
	Phase2	UMETA(DisplayName = "Phase 2"),
	Phase3	UMETA(DisplayName = "Phase 3")
};

/**
 * Up   - shield holding, incoming damage is heavily reduced (not blocked).
 * Down - hack 1 landed, boss shut down and taking full damage.
 */
UENUM(BlueprintType)
enum class EShieldState : uint8
{
	Up		UMETA(DisplayName = "Up"),
	Down	UMETA(DisplayName = "Down")
};

UENUM(BlueprintType)
enum class EHackType : uint8
{
	DropShield		UMETA(DisplayName = "Drop Boss Shield"),
	StunBoss		UMETA(DisplayName = "Stun Boss"),
	PlayerShield	UMETA(DisplayName = "Player Shield"),
	Heal			UMETA(DisplayName = "Heal")
};

/**
 * One player hack, as authored in the editor.
 *
 * Pure data - the generated key sequence and all runtime state live in the
 * hack component's parallel FHackRuntimeState array, so nothing writes back
 * into designer-authored values.
 */
USTRUCT(BlueprintType)
struct FHackDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hack")
	EHackType Type = EHackType::DropShield;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hack")
	FText DisplayName;

	/** Seconds the effect lasts. 0 for instant hacks such as Heal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hack", meta = (ClampMin = "0.0"))
	float Duration = 10.f;

	/** Seconds before it can be used again, counted from when Duration ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hack", meta = (ClampMin = "0.0"))
	float Cooldown = 10.f;

	/** How much this hack heals. Only read when Type == Heal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hack", meta = (ClampMin = "0.0"))
	float HealAmount = 30.f;
};
