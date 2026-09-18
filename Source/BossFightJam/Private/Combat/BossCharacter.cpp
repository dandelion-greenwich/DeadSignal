#include "Combat/BossCharacter.h"

#include "Combat/HealthComponent.h"
#include "Core/DeadSignalGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Combat/ProjectileBase.h"
#include "Combat/ProjectilePoolSubsystem.h"
#include "DrawDebugHelpers.h"
#include "NiagaraComponent.h"

ABossCharacter::ABossCharacter()
{
	// Ticks for facing player and laser sweep,  everything else runs on timers.
	PrimaryActorTick.bCanEverTick = true;
	
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RootComponent = Mesh;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->SetCapsuleSize(120.f, 250.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	Capsule->SetupAttachment(Mesh);

	// For weapon detection
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	LaserStart = CreateDefaultSubobject<USceneComponent>(TEXT("LaserStart"));
	LaserStart->SetupAttachment(Mesh);

	LaserBeam = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LaserBeam"));
	LaserBeam->SetupAttachment(LaserStart);

	// Off until a sweep starts, or the beam would fire the moment the level loads.
	LaserBeam->SetAutoActivate(false);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
}

void ABossCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (ProjectileClass)
	{
		if (UProjectilePoolSubsystem* Pool = GetWorld()->GetSubsystem<UProjectilePoolSubsystem>())
		{
			Pool->Prewarm(ProjectileClass, ProjectilePoolSize);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Boss] No ProjectileClass set - bullet patterns will fire nothing."));
	}

	if (Health)
	{
		Health->OnHealthChanged.AddDynamic(this, &ABossCharacter::HandleHealthChanged);

		// The shield starts up, so incoming damage starts reduced.
		Health->SetDamageMultiplier(ShieldedDamageMultiplier);
	}

	// Lets the game mode watch for our death without knowing our type.
	if (ADeadSignalGameMode* GameMode = Cast<ADeadSignalGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GameMode->RegisterBoss(this);
		GameMode->OnEncounterStateChanged.AddDynamic(this, &ABossCharacter::HandleEncounterStateChanged);

		// The encounter may already have started: the game mode calls
		// StartEncounter in its own BeginPlay, which can run before ours.
		HandleEncounterStateChanged(GameMode->GetEncounterState());
	}
}

// ---------------------------------------------------------------- Shield

void ABossCharacter::DropShield(float Duration)
{
	if (Duration <= 0.f)
	{
		return;
	}

	// Restart rather than stack - re-hacking mid-window resets the clock, it
	// does not grant a second window on top of the first.
	GetWorldTimerManager().ClearTimer(ShieldTimer);
	GetWorldTimerManager().SetTimer(ShieldTimer, this, &ABossCharacter::RestoreShield, Duration, false);

	SetShieldState(EShieldState::Down);
	
	if (Mesh)
		Mesh -> SetMaterial(0, InvisibleMaterial);
}

void ABossCharacter::RestoreShield()
{
	GetWorldTimerManager().ClearTimer(ShieldTimer);
	SetShieldState(EShieldState::Up);

	if (Mesh)
		Mesh ->SetMaterial(0, ShieldMaterial);

	UGameplayStatics::SpawnSoundAttached(ShieldUpSound, GetRootComponent());
}

void ABossCharacter::SetShieldState(EShieldState NewState)
{
	if (ShieldState == NewState)
	{
		return;
	}

	ShieldState = NewState;

	if (Health)
	{
		Health->SetDamageMultiplier(NewState == EShieldState::Down ? 1.f : ShieldedDamageMultiplier);
	}

	OnShieldStateChanged.Broadcast(NewState);

	if (NewState == EShieldState::Down)
	{
		OnShieldDropped();
	}
	else
	{
		OnShieldRestored();
	}
}

float ABossCharacter::GetRemainingShieldDownTime() const
{
	return FMath::Max(0.f, GetWorldTimerManager().GetTimerRemaining(ShieldTimer));
}


void ABossCharacter::ApplyStun(float Duration)
{
	if (Duration <= 0.f)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(StunTimer);
	GetWorldTimerManager().SetTimer(StunTimer, this, &ABossCharacter::EndStun, Duration, false);

	if (!bStunned)
	{
		bStunned = true;

		if (Mesh)
		{
			// Pauses anim asset
			Mesh->bPauseAnims = true;
		}

		OnStunChanged.Broadcast(true);
		OnStunStarted();
		UGameplayStatics::SpawnSoundAttached(StunSound, GetRootComponent());
	}

	// Cancel whatever was pending. Without this a stun landing mid-telegraph
	// would still let the attack fire, since only RunCurrentStep checks CanAct.
	if (bSequenceRunning)
	{
		GetWorldTimerManager().ClearTimer(AttackTimer);
		StopBulletPattern();
		StopLaserSweep();
		StopTeleportAttack();

		// StepIndex is left alone, so the interrupted attack runs again from its
		// telegraph once the stun ends - the player gets a fresh tell, and the
		// stun costs the boss the wind-up rather than skipping content.
		RunCurrentStep();

		ScreenMessage(FString::Printf(TEXT("STUNNED %.1fs - attack interrupted"), Duration), FColor::Red);
	}
	else
	{
		ScreenMessage(FString::Printf(TEXT("STUNNED %.1fs"), Duration), FColor::Red);
	}
}

void ABossCharacter::EndStun()
{
	if (!bStunned)
	{
		return;
	}

	bStunned = false;

	if (Mesh)
	{
		// Unpauses anim asset
		Mesh->bPauseAnims = false;
	}

	OnStunChanged.Broadcast(false);
	OnStunEnded();

	ScreenMessage(TEXT("stun over - resuming"), FColor::Red);
}


void ABossCharacter::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
	if (MaxHealth <= 0.f)
	{
		return;
	}

	const EBossPhase Target = PhaseForHealthPercent(CurrentHealth / MaxHealth);
	if (Target == CurrentPhase)
	{
		return;
	}

	// Advance one phase at a time even if a burst crossed two thresholds at
	// once - skipping a phase means content nobody ever sees. The next health
	// change re-runs this and steps again if still behind.
	const uint8 Next = static_cast<uint8>(CurrentPhase) + 1;
	if (Next <= static_cast<uint8>(Target))
	{
		EnterPhase(static_cast<EBossPhase>(Next));
	}
}

EBossPhase ABossCharacter::PhaseForHealthPercent(float Percent) const
{
	if (Percent <= Phase3Threshold)
	{
		return EBossPhase::Phase3;
	}
	if (Percent <= Phase2Threshold)
	{
		return EBossPhase::Phase2;
	}
	return EBossPhase::Phase1;
}

void ABossCharacter::EnterPhase(EBossPhase NewPhase)
{
	CurrentPhase = NewPhase;

	// Announce, do not ask. Whoever plays the transition sequence listens; the
	// boss never waits on them, so a missing sequence cannot deadlock the fight.
	OnPhaseChanged.Broadcast(NewPhase);
	OnPhaseTransitionStarted(NewPhase);

	bTransitioning = true;
	GetWorldTimerManager().ClearTimer(TransitionTimer);
	GetWorldTimerManager().SetTimer(TransitionTimer, this, &ABossCharacter::EndTransition,
		FMath::Max(TransitionDuration, 0.001f), false);

	UE_LOG(LogTemp, Log, TEXT("[Boss] Entered phase %d"), static_cast<int32>(NewPhase) + 1);

	ScreenMessage(FString::Printf(TEXT("PHASE %d  (%d attacks)"),
		static_cast<int32>(NewPhase) + 1, GetSequenceForPhase(NewPhase).Num()), FColor::Cyan);

	if (bSequenceRunning)
	{
		// Everything in flight belongs to the old phase. Clearing only the step
		// timer would leave a laser still sweeping and a bullet pattern still
		// emitting across the boundary and into the new phase.
		GetWorldTimerManager().ClearTimer(AttackTimer);
		StopBulletPattern();
		StopLaserSweep();
		StopTeleportAttack();

		StepIndex = 0;
		RunCurrentStep();
	}
}

void ABossCharacter::EndTransition()
{
	bTransitioning = false;
}

float ABossCharacter::ReceiveShot_Implementation(float Damage, AActor* DamageInstigator, const FHitResult& Hit)
{
	if (ShieldState == EShieldState::Up)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitShieldUpSound, Hit.ImpactPoint);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitShieldDownSound, Hit.ImpactPoint);
	}
	
	return Health ? Health->ApplyDamage(Damage, DamageInstigator) : 0.f;
}

#if !UE_BUILD_SHIPPING
void ABossCharacter::DebugSetPhase(EBossPhase NewPhase)
{
	if (NewPhase != CurrentPhase)
	{
		EnterPhase(NewPhase);
	}
}
#endif

bool ABossCharacter::CanAct() const
{
	return !bStunned
		&& !bTransitioning
		&& Health
		&& !Health->IsDead();
}

// ---------------------------------------------------------------- Encounter

void ABossCharacter::HandleEncounterStateChanged(EEncounterState NewState)
{
	CachedEncounterState = NewState;

	if (NewState == EEncounterState::Fighting)
	{
		StartAttackSequence();
	}
	else
	{
		// Intro, Victory and Defeat all mean stop - the boss should not be
		// firing during a cinematic or after the end screen.
		StopAttackSequence();
	}
}

// ---------------------------------------------------------------- Attacks

const TArray<FBossAttackStep>& ABossCharacter::GetSequenceForPhase(EBossPhase Phase) const
{
	switch (Phase)
	{
	case EBossPhase::Phase2: return Phase2Sequence;
	case EBossPhase::Phase3: return Phase3Sequence;
	default:                return Phase1Sequence;
	}
}

int32 ABossCharacter::GetCurrentSequenceLength() const
{
	return GetSequenceForPhase(CurrentPhase).Num();
}

EBossAttackType ABossCharacter::GetCurrentAttackType() const
{
	const TArray<FBossAttackStep>& Sequence = GetSequenceForPhase(CurrentPhase);
	return Sequence.IsValidIndex(StepIndex) ? Sequence[StepIndex].Type : EBossAttackType::BulletPattern;
}

void ABossCharacter::StartAttackSequence()
{
	if (bSequenceRunning)
	{
		return;
	}

	if (GetSequenceForPhase(CurrentPhase).Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Boss] Phase %d has an empty attack sequence - the boss will do nothing."),
			static_cast<int32>(CurrentPhase) + 1);
		return;
	}

	bSequenceRunning = true;
	StepIndex = 0;

	RunCurrentStep();
}

void ABossCharacter::StopAttackSequence()
{
	bSequenceRunning = false;
	GetWorldTimerManager().ClearTimer(AttackTimer);
	StopBulletPattern();
	StopLaserSweep();
	StopTeleportAttack();
}

void ABossCharacter::RunCurrentStep()
{
	if (!bSequenceRunning)
	{
		return;
	}

	const TArray<FBossAttackStep>& Sequence = GetSequenceForPhase(CurrentPhase);
	if (!Sequence.IsValidIndex(StepIndex))
	{
		StopAttackSequence();
		return;
	}

	// Stunned or mid-transition: wait rather than skip, so a stun costs the
	// boss time instead of silently eating an attack out of the order.
	if (!CanAct())
	{
		GetWorldTimerManager().SetTimer(AttackTimer, this, &ABossCharacter::RunCurrentStep,
			BlockedRetryInterval, false);
		return;
	}

	const FBossAttackStep& Step = Sequence[StepIndex];

	ScreenMessage(FString::Printf(TEXT("P%d  step %d/%d  %s  x%.1f"),
		static_cast<int32>(CurrentPhase) + 1,
		StepIndex + 1, Sequence.Num(),
		*UEnum::GetDisplayValueAsText(Step.Type).ToString(),
		Step.Intensity),
		FColor::Orange);

	OnAttackTelegraph(Step.Type, StepIndex, Step.Intensity);

	if (Step.TelegraphTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(AttackTimer, this, &ABossCharacter::ExecuteCurrentStep,
			Step.TelegraphTime, false);
	}
	else
	{
		ExecuteCurrentStep();
	}
}

void ABossCharacter::ExecuteCurrentStep()
{
	if (!bSequenceRunning)
	{
		return;
	}

	const TArray<FBossAttackStep>& Sequence = GetSequenceForPhase(CurrentPhase);
	if (!Sequence.IsValidIndex(StepIndex))
	{
		StopAttackSequence();
		return;
	}

	const FBossAttackStep& Step = Sequence[StepIndex];
	
	if (Step.Type == EBossAttackType::BulletPattern)
	{
		BeginBulletPattern(Step);
	}
	else if (Step.Type == EBossAttackType::Teleport)
	{
		PatternStep = Step;
		BeginTeleportAttack();
	}
	else if (Step.Type == EBossAttackType::LaserSweep)
	{
		PatternStep = Step;
		LaserAttackEndTime = GetWorld()->GetTimeSeconds() + Step.Duration;
		BeginLaserSweep();
	}

	OnAttackExecute(Step.Type, StepIndex, Step.Intensity, Step.GetRepeatCount());

	UE_LOG(LogTemp, Log, TEXT("[Boss] t=%.2f  P%d step %d/%d  %s  intensity %.2f (x%d)"),
		GetWorld()->GetTimeSeconds(),
		static_cast<int32>(CurrentPhase) + 1, StepIndex + 1, Sequence.Num(),
		*UEnum::GetValueAsString(Step.Type), Step.Intensity, Step.GetRepeatCount());

	GetWorldTimerManager().SetTimer(AttackTimer, this, &ABossCharacter::AdvanceStep,
		FMath::Max(Step.Duration + Step.Recovery, 0.05f), false);
}

void ABossCharacter::AdvanceStep()
{
	if (!bSequenceRunning)
	{
		return;
	}

	const TArray<FBossAttackStep>& Sequence = GetSequenceForPhase(CurrentPhase);

	++StepIndex;

	if (StepIndex >= Sequence.Num())
	{
		StepIndex = 0;

		ScreenMessage(FString::Printf(TEXT("P%d  sequence complete - looping"),
			static_cast<int32>(CurrentPhase) + 1), FColor::Yellow);

		OnSequenceCompleted(CurrentPhase);
	}

	RunCurrentStep();
}

void ABossCharacter::ScreenMessage(const FString& Message, const FColor Colour) const
{
#if !UE_BUILD_SHIPPING
	if (bShowDebugMessages && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, Colour, TEXT("[BOSS] ") + Message);
	}
#endif
}

// ---------------------------------------------------------------- Bullets

void ABossCharacter::BeginBulletPattern(const FBossAttackStep& Step)
{
	// Copied rather than referenced: the sequence advances while waves are
	// still in flight, so the step it points at can change underneath us.
	PatternStep = Step;
	PatternSpin = 0.f;
	bPatternFiring = true;

	// Emitting is bounded by the step's Duration rather than a wave count, so
	// how long an attack lasts is stated once and cannot drift out of sync.
	PatternEndTime = GetWorld()->GetTimeSeconds() + Step.Duration;

	FireBulletWave();
}

void ABossCharacter::StopBulletPattern()
{
	bPatternFiring = false;
	GetWorldTimerManager().ClearTimer(WaveTimer);
}

void ABossCharacter::FireBulletWave()
{
	if (!bPatternFiring)
	{
		return;
	}

	UProjectilePoolSubsystem* Pool = GetWorld() ? GetWorld()->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	if (!Pool)
	{
		StopBulletPattern();
		return;
	}

	const int32 BaseArms = FMath::Max(1, FMath::RoundToInt(PatternStep.Arms * PatternStep.Intensity));
	const int32 Bands = FMath::Max(1, PatternStep.PitchBands);

	const bool bFullCircle = PatternStep.YawArc >= 359.9f;
	const int32 BaseDivisions = bFullCircle ? BaseArms : FMath::Max(1, BaseArms - 1);
	const float BaseYawStep = PatternStep.YawArc / BaseDivisions;

	// The arc is centred on wherever the boss faces, so "in front of it" is
	// whatever the boss is currently pointing at.
	float CentreYaw = GetActorRotation().Yaw;
	float CentrePitch = GetActorRotation().Pitch;

	if (PatternStep.bAimAtPlayer)
	{
		if (const APawn* Player = GetWorld()->GetFirstPlayerController()
			? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr)
		{
			const FRotator ToPlayer = (Player->GetActorLocation() - GetActorLocation()).Rotation();
			CentreYaw = ToPlayer.Yaw;
			CentrePitch = ToPlayer.Pitch;
		}
		
		if (BaseYawStep > 0.f)
		{
			CentreYaw += FMath::Fmod(PatternSpin, BaseYawStep);
		}
	}
	else
	{
		// Free-aimed: let it accumulate, which is what turns a fan into a spiral.
		CentreYaw += PatternSpin;
	}

	for (int32 Band = 0; Band < Bands; ++Band)
	{
		// Bands spread either side of the aim, not either side of level.
		// Clamped short of straight up or down, where yaw stops meaning anything.
		const float BandAlpha = Bands > 1 ? static_cast<float>(Band) / (Bands - 1) : 0.5f;
		const float BandOffset = FMath::Lerp(-PatternStep.PitchArc, PatternStep.PitchArc, BandAlpha);
		const float Pitch = FMath::Clamp(CentrePitch + BandOffset, -89.f, 89.f);

		// A band near the extremes covers far less ground than one at level, so
		// evening the spread means thinning those bands by their cosine.
		int32 ArmsThisBand = BaseArms;

		// A full circle must not fire twice at the same heading, so the last
		// division is dropped when the arc closes on itself.
		const int32 Divisions = bFullCircle ? ArmsThisBand : FMath::Max(1, ArmsThisBand - 1);
		const float YawStep = Divisions > 0 ? PatternStep.YawArc / Divisions : 0.f;
		const float StartYaw = CentreYaw - (PatternStep.YawArc * 0.5f);

		for (int32 i = 0; i < ArmsThisBand; ++i)
		{
			const FRotator Direction(Pitch, ArmsThisBand > 1 ? StartYaw + YawStep * i : CentreYaw, 0.f);
			const FVector Origin = GetActorLocation() + Direction.Vector() * PatternStep.MuzzleOffset;

			Pool->Acquire(FTransform(Direction, Origin), this);
		}
	}

	PatternSpin += PatternStep.SpinPerWave;

	// Only schedule another if it would land inside the attack, so the pattern
	// never spills past the step it belongs to. A Duration of 0 therefore fires
	// exactly one wave, which is the sensible reading of an instant attack.
	const float Interval = FMath::Max(PatternStep.WaveInterval, 0.02f);

	if (GetWorld()->GetTimeSeconds() + Interval <= PatternEndTime)
	{
		GetWorldTimerManager().SetTimer(WaveTimer, this, &ABossCharacter::FireBulletWave, Interval, false);
	}
	else
	{
		bPatternFiring = false;
	}
}

// ---------------------------------------------------------------- Laser

void ABossCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bFacePlayer)
	{
		TickFacing(DeltaTime);
	}

	if (bLaserActive)
	{
		TickLaserSweep(DeltaTime);
	}
}

void ABossCharacter::TickFacing(float DeltaTime)
{
	// A stunned boss is inactive, and a finished fight should leave it wherever
	// it stopped rather than tracking the player through the end screen.
	if (bStunned
		|| CachedEncounterState == EEncounterState::Victory
		|| CachedEncounterState == EEncounterState::Defeat)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Player = PC ? PC->GetPawn() : nullptr;

	if (!Player)
	{
		return;
	}

	const float TargetYaw = (Player->GetActorLocation() - GetActorLocation()).Rotation().Yaw;

	// FixedTurn handles wrapping past 180, so the boss always turns the short
	// way round instead of spinning most of a circle to reach a nearby angle.
	const float NewYaw = FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, FacingTurnRate * DeltaTime);

	// Yaw only - pitch and roll stay flat so the boss never tips over.
	SetActorRotation(FRotator(0.f, NewYaw, 0.f));
}

void ABossCharacter::BeginLaserSweep()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Captured now and then fixed. The beam does not track, so the sweep is
	// dodgeable - but each new sweep re-captures, so standing still is not.
	const FVector Start = GetLaserStartLocation();

	LaserYaw = GetActorRotation().Yaw;

	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		if (const APawn* Player = PC->GetPawn())
		{
			LaserYaw = (Player->GetActorLocation() - Start).Rotation().Yaw;
		}
	}

	// Always starts pointing straight down, so the contact point begins under
	// the boss and races outward as the pitch rises.
	LaserPitch = -90.f;

	LaserSweepEndTime = World->GetTimeSeconds() + FMath::Max(PatternStep.LaserDuration, 0.1f);
	bLaserActive = true;

	if (LaserBeam)
	{
		// Collapsed onto the start BEFORE activating
		LaserBeam->SetVariableVec3(BeamEndParameter, Start);

		// Reset so every sweep starts clean rather than inheriting the last one's particles.
		LaserBeam->Activate(true);
	}

	const float SweepDuration = PatternStep.LaserDuration;
	USoundBase* LaserSound = nullptr;

	if (SweepDuration >= 1.f)
	{
		LaserSound = Laser1s;
	}
	else if (SweepDuration >= 0.75f)
	{
		LaserSound = Laser075s;
	}
	else if (SweepDuration >= 0.5f)
	{
		LaserSound = Laser05s;
	}

	if (LaserSound)
	{
		UGameplayStatics::SpawnSoundAttached(LaserSound, LaserStart ? LaserStart.Get() : GetRootComponent());
	}

	OnLaserStarted();
}

void ABossCharacter::StopLaserSweep()
{
	if (bLaserActive)
	{
		bLaserActive = false;
		OnLaserFinished();
	}
	
	if (LaserBeam)
	{
		LaserBeam->Deactivate();
	}

	GetWorldTimerManager().ClearTimer(LaserIntervalTimer);
}

FVector ABossCharacter::GetLaserStartLocation() const
{
	return LaserStart ? LaserStart->GetComponentLocation() : GetActorLocation();
}

void ABossCharacter::TickLaserSweep(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	if (Now >= LaserSweepEndTime)
	{
		bLaserActive = false;

		if (LaserBeam)
		{
			LaserBeam->Deactivate();
		}

		OnLaserFinished();

		// Another sweep if the step still has time, re-capturing the player.
		if (Now < LaserAttackEndTime)
		{
			GetWorldTimerManager().SetTimer(LaserIntervalTimer, this, &ABossCharacter::BeginLaserSweep,
				FMath::Max(PatternStep.LaserInterval, 0.01f), false);
		}
		return;
	}

	// Rotating up from straight down. Speed times duration is the arc covered,
	// so how far past the player it reaches falls out of those two.
	LaserPitch += PatternStep.LaserSweepSpeed * DeltaTime;
	LaserPitch = FMath::Clamp(LaserPitch, -90.f, 89.f);

	const FRotator Direction(LaserPitch, LaserYaw, 0.f);
	const FVector Start = GetLaserStartLocation();
	FVector End = Start + Direction.Vector() * PatternStep.LaserRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BossLaser), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(this);

	// Geometry stops the beam, so it does not shine through the arena walls.
	FHitResult GeometryHit;
	if (World->LineTraceSingleByChannel(GeometryHit, Start, End, ECC_Visibility, Params))
	{
		End = GeometryHit.ImpactPoint;
	}

	if (LaserBeam)
	{
		// Every frame, not only on a hit: End is the impact point when something
		// blocked the trace and the full range when nothing did. Updating only on
		// hits would freeze the beam at its last contact once it swept past an edge.
		LaserBeam->SetVariableVec3(BeamEndParameter, End);
	}

	FHitResult PawnHit;
	if (World->LineTraceSingleByChannel(PawnHit, Start, End, ECC_Pawn, Params))
	{
		if (AActor* HitActor = PawnHit.GetActor())
		{
			if (UHealthComponent* PlayerHealth = HitActor->FindComponentByClass<UHealthComponent>())
			{
				PlayerHealth->ApplyDamage(PatternStep.LaserDamage, this);
			}
		}
	}

	const float SweepAlpha = FMath::GetRangePct(LaserSweepEndTime - PatternStep.LaserDuration, LaserSweepEndTime, Now);
	OnLaserUpdated(Start, End, FMath::Clamp(SweepAlpha, 0.f, 1.f));

#if ENABLE_DRAW_DEBUG
	if (bShowDebugMessages)
	{
		DrawDebugLine(World, Start, End, FColor::Red, false, -1.f, 0, 4.f);
	}
#endif
}

// ---------------------------------------------------------------- Teleport

void ABossCharacter::BeginTeleportAttack()
{
	if (TeleportAnchors.Num() < 2)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Boss] Teleport needs at least 2 anchors on the placed boss - %d set, skipping."),
			TeleportAnchors.Num());
		return;
	}

	TeleportsRemaining = PatternStep.GetRepeatCount();

	// Spread evenly across the step, so raising Intensity makes the boss blink
	// faster within the same window rather than needing a second field.
	TeleportInterval = PatternStep.Duration / FMath::Max(TeleportsRemaining, 1);
	
	// The first one lands immediately; Duration paces the rest.
	DoTeleport();
}

void ABossCharacter::StopTeleportAttack()
{
	TeleportsRemaining = 0;
	GetWorldTimerManager().ClearTimer(TeleportTimer);
}

void ABossCharacter::DoTeleport()
{
	if (TeleportsRemaining <= 0)
	{
		return;
	}

	// Every anchor except the one we are standing on, so the boss is guaranteed
	// to actually move rather than occasionally blinking in place.
	TArray<int32> Candidates;
	Candidates.Reserve(TeleportAnchors.Num());

	for (int32 i = 0; i < TeleportAnchors.Num(); ++i)
	{
		if (i != CurrentAnchorIndex && TeleportAnchors[i])
		{
			Candidates.Add(i);
		}
	}

	if (Candidates.Num() == 0)
	{
		StopTeleportAttack();
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(this, TeleportSound, GetActorLocation());

	const int32 ChosenIndex = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];

	const FVector From = GetActorLocation();
	const FVector To = TeleportAnchors[ChosenIndex]->GetActorLocation();

	// Location only - facing is driven by the player, not by the anchor.
	SetActorLocation(To, false, nullptr, ETeleportType::TeleportPhysics);
	CurrentAnchorIndex = ChosenIndex;

	OnTeleported(From, To);

	ScreenMessage(FString::Printf(TEXT("teleport -> %s"),
		*GetNameSafe(TeleportAnchors[ChosenIndex])), FColor::Magenta);

	--TeleportsRemaining;

	if (TeleportsRemaining > 0)
	{
		GetWorldTimerManager().SetTimer(TeleportTimer, this, &ABossCharacter::DoTeleport,
			FMath::Max(TeleportInterval, 0.05f), false);
	}
}
