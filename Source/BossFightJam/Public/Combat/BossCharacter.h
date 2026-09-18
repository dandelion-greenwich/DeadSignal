#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/DeadSignalTypes.h"
#include "Core/Damageable.h"
#include "Core/DeadSignalGameMode.h"
#include "BossCharacter.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UHealthComponent;
class UNiagaraComponent;
class AProjectileBase;


UENUM(BlueprintType)
enum class EBossAttackType : uint8
{
	BulletPattern	UMETA(DisplayName = "Bullet Pattern"),
	LaserSweep		UMETA(DisplayName = "Laser Sweep"),
	Teleport		UMETA(DisplayName = "Teleport"),
	EMP				UMETA(DisplayName = "EMP"),
	HackInvasion	UMETA(DisplayName = "Hack Invasion"),
	Pulse			UMETA(DisplayName = "Pulse Knockback")
};

/** One step in a phase's attack order. */
USTRUCT(BlueprintType)
struct FBossAttackStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	EBossAttackType Type = EBossAttackType::BulletPattern;

	/** Telegraph before the attack fires, so it can be read and dodged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	float TelegraphTime = 0.75f;

	/** How long the attack itself lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	float Duration = 1.5f;

	/** Idle time after it finishes, before the next step begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.0"))
	float Recovery = 1.f;

	/**
	 * How much of this attack to produce, relative to its base. Lets the same
	 * attack type appear in every phase and simply do more each time.
	 *
	 * Each attack reads it in its own natural way:
	 *   Bullet Pattern - multiplies the bullet count
	 *   Laser Sweep    - number of sweeps
	 *   Teleport       - number of hops
	 *   EMP            - multiplies the blackout duration
	 *   Pulse          - multiplies knockback strength
	 *
	 * 1.0 is the attack's designed baseline, so phase 1 usually leaves it alone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float Intensity = 1.f;

	/**
	 * Intensity as a whole number, for the attacks that repeat rather than
	 * scale - sweeps, teleports. One conversion in one place, so 2.5 cannot
	 * mean three hops in one attack and two in another.
	 */
	int32 GetRepeatCount() const { return FMath::Max(1, FMath::RoundToInt(Intensity)); }

	// ---- Bullet Pattern only. Ignored by every other type. ----

	/** Bullets per band, spread across YawArc. Scaled by Intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Bullet Pattern", meta = (ClampMin = "1"))
	int32 Arms = 7;

	/** Horizontal spread, centred on the boss's facing. 180 is a forward half-circle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Bullet Pattern", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float YawArc = 180.f;

	/** Horizontal slices stacked vertically. 1 is a flat fan. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Bullet Pattern", meta = (ClampMin = "1"))
	int32 PitchBands = 3;

	/** Vertical spread either side of level, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Bullet Pattern", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float PitchArc = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Bullet Pattern", meta = (ClampMin = "0.02"))
	float WaveInterval = 0.25f;

	/** Degrees the fan rotates between waves. 0 gives straight rays, anything else spirals. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Bullet Pattern")
	float SpinPerWave = 0.f;

	/** Distance in front of the boss that bullets appear. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Bullet Pattern", meta = (ClampMin = "0.0"))
	float MuzzleOffset = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Bullet Pattern")
	bool bAimAtPlayer = true;

	/** How long one sweep lasts. Several run back to back across the step's Duration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Laser Sweep", meta = (ClampMin = "0.1"))
	float LaserDuration = 1.5f;

	/**
	 * Degrees per second the beam rotates upward from straight down.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Laser Sweep", meta = (ClampMin = "1.0"))
	float LaserSweepSpeed = 70.f;

	/** Gap between one sweep ending and the next starting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Laser Sweep", meta = (ClampMin = "0.0"))
	float LaserInterval = 0.25f;

	/**
	 * Damage per contact, not per second.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Laser Sweep", meta = (ClampMin = "0.0"))
	float LaserDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Laser Sweep", meta = (ClampMin = "100.0"))
	float LaserRange = 6000.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChanged, EBossPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShieldStateChanged, EShieldState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStunChanged, bool, bStunned);

UCLASS()
class BOSSFIGHTJAM_API ABossCharacter : public APawn, public IDamageable
{
	GENERATED_BODY()

public:
	ABossCharacter();

	
	// IDamageable. Forwards to the health component
	virtual float ReceiveShot_Implementation(float Damage, AActor* DamageInstigator, const FHitResult& Hit) override;
	
	UFUNCTION(BlueprintCallable, Category = "Boss|Shield")
	void DropShield(float Duration);

	/** Forces the shield back up immediately, cancelling any active window. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Shield")
	void RestoreShield();

	/** Halts boss behaviour for Duration seconds. Called by the player's hack 2. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Stun")
	void ApplyStun(float Duration);

	UFUNCTION(BlueprintPure, Category = "Boss|Shield")
	EShieldState GetShieldState() const { return ShieldState; }

	UFUNCTION(BlueprintPure, Category = "Boss|Shield")
	bool IsShieldDown() const { return ShieldState == EShieldState::Down; }

	/** Seconds left in the current damage window, 0 if the shield is up. */
	UFUNCTION(BlueprintPure, Category = "Boss|Shield")
	float GetRemainingShieldDownTime() const;

	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	EBossPhase GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	bool IsTransitioning() const { return bTransitioning; }

	UFUNCTION(BlueprintPure, Category = "Boss|Stun")
	bool IsStunned() const { return bStunned; }

	/** False whenever the boss should not be acting - stunned, transitioning or shut down. */
	UFUNCTION(BlueprintPure, Category = "Boss")
	bool CanAct() const;

	UFUNCTION(BlueprintPure, Category = "Boss")
	UHealthComponent* GetHealthComponent() const { return Health; }

	/** Begins running the current phase's attack order. Idempotent. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Attacks")
	void StartAttackSequence();

	/** Halts the scheduler. Called when the encounter ends. */
	UFUNCTION(BlueprintCallable, Category = "Boss|Attacks")
	void StopAttackSequence();

	UFUNCTION(BlueprintPure, Category = "Boss|Attacks")
	bool IsAttacking() const { return bSequenceRunning; }

	/** Position in the current phase's sequence, for debugging. */
	UFUNCTION(BlueprintPure, Category = "Boss|Attacks")
	int32 GetCurrentStepIndex() const { return StepIndex; }

	UFUNCTION(BlueprintPure, Category = "Boss|Attacks")
	int32 GetCurrentSequenceLength() const;

	/** The step currently running. Meaningless while IsAttacking is false. */
	UFUNCTION(BlueprintPure, Category = "Boss|Attacks")
	EBossAttackType GetCurrentAttackType() const;

#if !UE_BUILD_SHIPPING
	// Debug only 
	void DebugSetPhase(EBossPhase NewPhase);
#endif

	UPROPERTY(BlueprintAssignable, Category = "Boss|Phase")
	FOnPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Shield")
	FOnShieldStateChanged OnShieldStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Boss|Stun")
	FOnStunChanged OnStunChanged;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** FX hooks for BP_Hiramor. C++ never needs to know what they do. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Shield")
	void OnShieldDropped();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Shield")
	void OnShieldRestored();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Phase")
	void OnPhaseTransitionStarted(EBossPhase NewPhase);

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Stun")
	void OnStunStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Stun")
	void OnStunEnded();

	/**
	 * Fires when a step begins its telegraph. Play the wind-up here - scale it
	 * by Intensity so a bigger attack looks bigger before it lands.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Attacks")
	void OnAttackTelegraph(EBossAttackType Type, int32 StepIndexInSequence, float Intensity);

	/**
	 * Fires when the attack actually goes off. Spawn projectiles here, reading
	 * Intensity for how many - see FBossAttackStep for what it means per type.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Attacks")
	void OnAttackExecute(EBossAttackType Type, int32 StepIndexInSequence, float Intensity, int32 RepeatCount);

	/** One sweep is beginning. Spawn the beam effect here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Attacks")
	void OnLaserStarted();

	/** Every frame of a sweep. Position the beam between these two points. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Attacks")
	void OnLaserUpdated(const FVector& Start, const FVector& End, float SweepAlpha);

	/** The sweep ended. Another may follow if the step still has time left. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Attacks")
	void OnLaserFinished();

	/** The boss just moved. Play the warp effect at both ends here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Teleport")
	void OnTeleported(const FVector& From, const FVector& To);

	/** Fires once the whole phase order has been run, before it loops. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|Attacks")
	void OnSequenceCompleted(EBossPhase Phase);

	/** The attack order for each phase, run start to finish then looped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Attacks")
	TArray<FBossAttackStep> Phase1Sequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Attacks")
	TArray<FBossAttackStep> Phase2Sequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Attacks")
	TArray<FBossAttackStep> Phase3Sequence;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss|Teleport")
	TArray<TObjectPtr<AActor>> TeleportAnchors;

	/** Turn to face the player continuously. Yaw only - the boss never tilts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Boss|Facing")
	bool bFacePlayer = true;

	/** Degrees per second. Low enough and a strafing player can outrun its aim. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Boss|Facing", meta = (ClampMin = "1.0"))
	float FacingTurnRate = 180.f;

	/** Pooled and reused for every bullet the boss fires. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Attacks")
	TSubclassOf<AProjectileBase> ProjectileClass;

	/** Prewarmed at BeginPlay so the first volley does not hitch. */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Attacks", meta = (ClampMin = "0"))
	int32 ProjectilePoolSize = 200;

	/** Seconds to wait before retrying when stunned or transitioning. */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Attacks", meta = (ClampMin = "0.05"))
	float BlockedRetryInterval = 0.25f;

	/** Prints phase changes, each attack, and sequence completion to the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Debug")
	bool bShowDebugMessages = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<USkeletalMeshComponent> Mesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<USceneComponent> LaserStart;

	/** Attached to LaserStart, so the effect always originates where the trace does. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UNiagaraComponent> LaserBeam;

	/** Niagara user parameter that receives the beam's end point each frame. */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Visuals")
	FName BeamEndParameter = TEXT("Beam End");

	UPROPERTY(EditAnywhere, Category = "Boss|Visuals")
	TObjectPtr<UMaterialInterface> ShieldMaterial;

	UPROPERTY(EditAnywhere, Category = "Boss|Visuals")
	TObjectPtr<UMaterialInterface> InvisibleMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UHealthComponent> Health;

	/**
	 * Damage scale while the shield holds. 0.2 means shots do a fifth of their
	 * damage - enough that shooting a shielded boss is not pointless, little
	 * enough that dropping the shield is clearly the right play.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Shield", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShieldedDamageMultiplier = 0.2f;

	/** Health fractions at which phases 2 and 3 begin. */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Phase2Threshold = 0.67f;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Phase3Threshold = 0.34f;

	/** Seconds the boss is inert while changing phase. */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Phase", meta = (ClampMin = "0.0"))
	float TransitionDuration = 2.f;

private:
	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth);
	void SetShieldState(EShieldState NewState);
	void EnterPhase(EBossPhase NewPhase);
	void EndTransition();
	void EndStun();

	/** Phase implied by a health fraction, ignoring what phase we are in now. */
	EBossPhase PhaseForHealthPercent(float Percent) const;

	UPROPERTY(VisibleInstanceOnly, Category = "Boss", meta = (AllowPrivateAccess = "true"))
	EBossPhase CurrentPhase = EBossPhase::Phase1;

	UPROPERTY(VisibleInstanceOnly, Category = "Boss", meta = (AllowPrivateAccess = "true"))
	EShieldState ShieldState = EShieldState::Up;

	UFUNCTION()
	void HandleEncounterStateChanged(EEncounterState NewState);

	/** Runs the step at StepIndex: telegraph, then execute, then schedule next. */
	void RunCurrentStep();
	void ExecuteCurrentStep();
	void AdvanceStep();

	const TArray<FBossAttackStep>& GetSequenceForPhase(EBossPhase Phase) const;
	void ScreenMessage(const FString& Message, const FColor Colour) const;

	/** Begins a bullet pattern: fires the first wave and schedules the rest. */
	void BeginBulletPattern(const FBossAttackStep& Step);

	/** Emits one fan - PitchBands slices, each Arms wide across YawArc. */
	void FireBulletWave();

	void StopBulletPattern();

	/** Captures the player and begins one sweep from straight down. */
	void BeginLaserSweep();

	/** Advances the beam, traces it, and damages anything it touches. */
	void TickLaserSweep(float DeltaTime);

	/** LaserStart's location, or the actor's if the component is missing. */
	FVector GetLaserStartLocation() const;

	void StopLaserSweep();

	/** Spreads Intensity teleports evenly across the step's Duration. */
	void BeginTeleportAttack();

	/** Moves to a random anchor that is not the current one. */
	void DoTeleport();

	void StopTeleportAttack();

	/** Yaw-only turn toward the player at FacingTurnRate. */
	void TickFacing(float DeltaTime);

	/** The step currently emitting, copied so the sequence can move on safely. */
	FBossAttackStep PatternStep;
	bool bPatternFiring = false;

	/** World time the pattern stops emitting - the step's Duration from its start. */
	float PatternEndTime = 0.f;
	float PatternSpin = 0.f;
	FTimerHandle WaveTimer;

	bool bLaserActive = false;

	/** World time the whole laser step ends - bounds how many sweeps run. */
	float LaserAttackEndTime = 0.f;

	/** World time this individual sweep ends. */
	float LaserSweepEndTime = 0.f;

	/** Captured when the sweep starts, then fixed - the beam does not track. */
	float LaserYaw = 0.f;
	float LaserPitch = -90.f;

	FTimerHandle LaserIntervalTimer;

	/** Cached so facing does not look the game mode up every frame. */
	EEncounterState CachedEncounterState = EEncounterState::Intro;

	int32 TeleportsRemaining = 0;
	float TeleportInterval = 0.f;

	/** Which anchor the boss is standing on, so it never picks the same one. */
	int32 CurrentAnchorIndex = INDEX_NONE;

	FTimerHandle TeleportTimer;

	bool bTransitioning = false;
	bool bStunned = false;
	bool bSequenceRunning = false;
	int32 StepIndex = 0;

	FTimerHandle ShieldTimer;
	FTimerHandle TransitionTimer;
	FTimerHandle StunTimer;
	FTimerHandle AttackTimer;

	UPROPERTY(EditAnywhere, Category = "Boss|Audio")
	TObjectPtr<USoundBase> HitShieldUpSound;
	
	UPROPERTY(EditAnywhere, Category = "Boss|Audio")
	TObjectPtr<USoundBase> HitShieldDownSound;

	UPROPERTY(EditAnywhere, Category = "Boss|Audio")
	TObjectPtr<USoundBase> ShieldUpSound;

	UPROPERTY(EditAnywhere, Category = "Boss|Audio")
	TObjectPtr<USoundBase> StunSound;

	/** Laser sweep sounds, picked by the sweep's LaserDuration. */
	UPROPERTY(EditAnywhere, Category = "Boss|Audio")
	TObjectPtr<USoundBase> Laser1s;

	UPROPERTY(EditAnywhere, Category = "Boss|Audio")
	TObjectPtr<USoundBase> Laser075s;

	UPROPERTY(EditAnywhere, Category = "Boss|Audio")
	TObjectPtr<USoundBase> Laser05s;
	
	UPROPERTY(EditAnywhere, Category = "Boss|Audio")
	TObjectPtr<USoundBase> TeleportSound;
};
