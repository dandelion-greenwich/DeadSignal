#include "Combat/HackComponent.h"

#include "Combat/BossCharacter.h"
#include "Combat/HealthComponent.h"
#include "Core/DeadSignalGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"

UHackComponent::UHackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHackComponent::BeginPlay()
{
	Super::BeginPlay();

	// Sized once from the authored array, so indices can never desync.
	Runtime.SetNum(Hacks.Num());

	// Generated up front so a panel built before the first open
	RegeneratePatterns();

	if (AActor* Owner = GetOwner())
	{
		ShieldMesh = Cast<UMeshComponent>(ShieldMeshReference.GetComponent(Owner));
	}

	// The shield starts down, so the mesh starts wearing the invisible material
	// rather than relying on whatever was set in the editor.
	SetShieldVisible(false);
}

// ---------------------------------------------------------------- Panel

void UHackComponent::TogglePanel()
{
	if (bPanelOpen)
	{
		ClosePanel();
	}
	else
	{
		OpenPanel();
	}
}

void UHackComponent::OpenPanel()
{
	if (bPanelOpen || bInputLocked)
	{
		return;
	}

	bPanelOpen = true;
	ClearInput();
	RegeneratePatterns();

	UGameplayStatics::PlaySound2D(this, OpenHackSound, 1.f, 1.f, 0.f,
	 nullptr,nullptr, false);

	OnPanelToggled.Broadcast(true);
}

void UHackComponent::ClosePanel()
{
	if (!bPanelOpen)
	{
		return;
	}

	bPanelOpen = false;
	ClearInput();

	UGameplayStatics::PlaySound2D(this, CloseHackSound, 1.f, 1.f, 0.f,
 nullptr,nullptr, false);

	OnPanelToggled.Broadcast(false);
}

void UHackComponent::SetInputLocked(bool bNewLocked)
{
	bInputLocked = bNewLocked;

	if (bInputLocked && bPanelOpen)
	{
		ClosePanel();
	}
}

// Patterns

void UHackComponent::RegeneratePatterns()
{
	TArray<TArray<int32>> Chosen;
	Chosen.Reserve(Hacks.Num());

	const int32 Length = FMath::Max(1, SequenceLength);

	for (int32 i = 0; i < Hacks.Num(); ++i)
	{
		const TArray<int32> Previous = Runtime[i].KeySequence;

		// Rejection sampling. With 4^Length combinations and only four hacks
		// this converges immediately; the cap only guards a pathological
		// SequenceLength of 1, where four distinct sequences barely exist.
		TArray<int32> Candidate;
		const int32 MaxAttempts = 200;

		for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
		{
			Candidate.Reset();
			for (int32 k = 0; k < Length; ++k)
			{
				Candidate.Add(FMath::RandRange(1, 4));
			}

			// Must differ from this hack's previous sequence, and from every
			// sequence already handed out this open. Because all sequences are
			// the same length and distinct, none can be a prefix of another -
			// so the grey-out filter stays unambiguous for free.
			if (Candidate == Previous)
			{
				continue;
			}
			if (Chosen.Contains(Candidate))
			{
				continue;
			}
			break;
		}

		Chosen.Add(Candidate);
		Runtime[i].KeySequence = Candidate;
	}

	OnPatternsRegenerated.Broadcast();
}

// Input

bool UHackComponent::CanAcceptInput() const
{
	if (!bPanelOpen || bInputLocked)
	{
		return false;
	}

	// No hacking before the fight starts or after it ends.
	if (const ADeadSignalGameMode* GameMode = Cast<ADeadSignalGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		if (!GameMode->IsFighting())
		{
			return false;
		}
	}

	return true;
}

void UHackComponent::SubmitKey(int32 Key)
{
	if (!CanAcceptInput() || Key < 1 || Key > 4)
	{
		return;
	}

	CurrentInput.Add(Key);

	// Which hacks still match what has been typed?
	int32 ExactMatch = INDEX_NONE;
	int32 ViableCount = 0;

	for (int32 i = 0; i < Hacks.Num(); ++i)
	{
		if (!IsHackViable(i))
		{
			continue;
		}

		++ViableCount;

		if (Runtime[i].KeySequence.Num() == CurrentInput.Num())
		{
			ExactMatch = i;
		}
	}

	if (ViableCount == 0)
	{
		FailInput();
		return;
	}

	if (ExactMatch != INDEX_NONE)
	{
		ActivateHack(ExactMatch);
		return;
	}

	UGameplayStatics::PlaySound2D(this, TypingSound, 1.f, 1.f, 0.f,
 nullptr,nullptr, false);

	OnInputChanged.Broadcast();
}

void UHackComponent::FailInput()
{
	ClearInput();
	OnHackFailed.Broadcast();
}

void UHackComponent::ClearInput()
{
	CurrentInput.Reset();
	OnInputChanged.Broadcast();
}

// Activation

void UHackComponent::ActivateHack(int32 Index)
{
	if (!IsValidIndex(Index) || !GetWorld())
	{
		return;
	}

	const FHackDefinition& Hack = Hacks[Index];
	FHackRuntimeState& State = Runtime[Index];

	ApplyHackEffect(Hack);

	// State is set BEFORE anything broadcasts. ClearInput and OnHackActivated
	// both cause listeners to re-read this hack, and if bActive were still
	// false at that point the UI would briefly show the row as idle.
	if (Hack.Duration > 0.f)
	{
		State.bActive = true;

		FTimerDelegate Expire;
		Expire.BindWeakLambda(this, [this, Index]() { HandleHackExpired(Index); });
		GetWorld()->GetTimerManager().SetTimer(State.DurationTimer, Expire, Hack.Duration, false);
	}
	else
	{
		// Instant hacks such as Heal have nothing to expire, so their cooldown starts here
		
		HandleHackExpired(Index);
	}

	ClearInput();

	OnHackActivated.Broadcast(Hack.Type, Index);

	UE_LOG(LogTemp, Log, TEXT("[Hacks] t=%.2f  ACTIVATED index %d %s (dur %.1fs, cd %.1fs)"),
		GetWorld()->GetTimeSeconds(), Index, *UEnum::GetValueAsString(Hack.Type),
		Hack.Duration, Hack.Cooldown);

#if !UE_BUILD_SHIPPING
	if (bShowDebugMessages && GEngine)
	{
		const FString Name = Hack.DisplayName.IsEmpty()
			? UEnum::GetDisplayValueAsText(Hack.Type).ToString()
			: Hack.DisplayName.ToString();

		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan,
			FString::Printf(TEXT("HACK %d: %s  (%.1fs / CD %.1fs)"),
				Index + 1, *Name, Hack.Duration, Hack.Cooldown));
	}
#endif
}

void UHackComponent::HandleHackExpired(int32 Index)
{
	if (!IsValidIndex(Index) || !GetWorld())
	{
		return;
	}

	const FHackDefinition& Hack = Hacks[Index];
	FHackRuntimeState& State = Runtime[Index];

	const bool bWasActive = State.bActive;

	State.bActive = false;

	// The duration handle is spent - drop it so nothing can read a stale value
	// off a slot the timer manager may reuse.
	State.DurationTimer.Invalidate();

	if (bWasActive)
	{
		RemoveHackEffect(Hack);
	}

	// The cooldown starts here rather than at activation, so an active hack
	// does not burn its own downtime.
	//
	// Critically this happens BEFORE the broadcast: listeners refresh in
	// response, and if bOnCooldown were still false they would briefly see the
	// hack as available and paint it idle until the next unrelated event.
	if (Hack.Cooldown > 0.f)
	{
		State.bOnCooldown = true;

		FTimerDelegate Ready;
		Ready.BindWeakLambda(this, [this, Index]() { HandleCooldownFinished(Index); });
		GetWorld()->GetTimerManager().SetTimer(State.CooldownTimer, Ready, Hack.Cooldown, false);
	}

	if (bWasActive)
	{
		UE_LOG(LogTemp, Log, TEXT("[Hacks] t=%.2f  EXPIRED index %d (duration was %.1fs)"),
			GetWorld()->GetTimeSeconds(), Index, Hack.Duration);

		OnHackExpired.Broadcast(Hack.Type, Index);
	}
}

void UHackComponent::HandleCooldownFinished(int32 Index)
{
	if (!IsValidIndex(Index))
	{
		return;
	}

	// Cleared before the broadcast. Listeners refresh in response, and the
	// timer that just fired does not reliably report as expired from inside
	// its own callback.
	Runtime[Index].bOnCooldown = false;
	Runtime[Index].CooldownTimer.Invalidate();

	UE_LOG(LogTemp, Log, TEXT("[Hacks] t=%.2f  COOLDOWN OVER index %d"),
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f, Index);

	OnCooldownFinished.Broadcast(Hacks[Index].Type, Index);
}

// Effects

void UHackComponent::ApplyHackEffect(const FHackDefinition& Hack)
{
	switch (Hack.Type)
	{
	case EHackType::DropShield:
	{
		if (ABossCharacter* Boss = GetBoss())
		{
			Boss->DropShield(Hack.Duration);
		}
		break;
	}

	case EHackType::StunBoss:
	{
		if (ABossCharacter* Boss = GetBoss())
		{
			Boss->ApplyStun(Hack.Duration);
		}
		break;
	}

	case EHackType::PlayerShield:
	{
		if (UHealthComponent* Health = GetOwnerHealth())
		{
			Health->SetInvulnerable(true);
		}

		UGameplayStatics::PlaySound2D(this, ShieldUpSound, 1.f, 1.f, 0.f,
nullptr,nullptr, false);
		SetShieldVisible(true);
		break;
	}

	case EHackType::Heal:
	{
		if (UHealthComponent* Health = GetOwnerHealth())
		{
			Health->Heal(Hack.HealAmount);
		}
		break;
	}
	}
}

void UHackComponent::RemoveHackEffect(const FHackDefinition& Hack)
{
	// Removes player's shield
	if (Hack.Type == EHackType::PlayerShield)
	{
		if (UHealthComponent* Health = GetOwnerHealth())
		{
			Health->SetInvulnerable(false);
		}

		SetShieldVisible(false);
	}
}

void UHackComponent::SetShieldVisible(bool bVisible)
{
	UMaterialInterface* Material = bVisible ? ShieldMaterial : InvisibleMaterial;

	if (!ShieldMesh || !Material)
	{
		return;
	}
	
	ShieldMesh->SetMaterial(0, Material);
}

#if !UE_BUILD_SHIPPING
void UHackComponent::DebugResetCooldowns()
{
	if (!GetWorld())
	{
		return;
	}

	for (int32 i = 0; i < Runtime.Num(); ++i)
	{
		FHackRuntimeState& State = Runtime[i];

		// Effects are removed properly rather than just cleared, or a reset
		// during Player Shield would leave the player permanently invulnerable.
		if (State.bActive && Hacks.IsValidIndex(i))
		{
			RemoveHackEffect(Hacks[i]);
		}

		GetWorld()->GetTimerManager().ClearTimer(State.DurationTimer);
		GetWorld()->GetTimerManager().ClearTimer(State.CooldownTimer);
		State.DurationTimer.Invalidate();
		State.CooldownTimer.Invalidate();

		State.bActive = false;
		State.bOnCooldown = false;
	}

	ClearInput();

	UE_LOG(LogTemp, Log, TEXT("[Hacks] Debug reset - all hacks available."));
}
#endif

// Queries

FHackDefinition UHackComponent::GetHackDefinition(int32 Index) const
{
	return Hacks.IsValidIndex(Index) ? Hacks[Index] : FHackDefinition();
}

TArray<int32> UHackComponent::GetKeySequence(int32 Index) const
{
	return Runtime.IsValidIndex(Index) ? Runtime[Index].KeySequence : TArray<int32>();
}

bool UHackComponent::IsHackViable(int32 Index) const
{
	if (!IsValidIndex(Index) || IsHackActive(Index) || IsOnCooldown(Index))
	{
		return false;
	}

	const TArray<int32>& Sequence = Runtime[Index].KeySequence;
	if (CurrentInput.Num() > Sequence.Num())
	{
		return false;
	}

	for (int32 i = 0; i < CurrentInput.Num(); ++i)
	{
		if (Sequence[i] != CurrentInput[i])
		{
			return false;
		}
	}

	return true;
}

bool UHackComponent::IsHackActive(int32 Index) const
{
	return IsValidIndex(Index) && Runtime[Index].bActive;
}

bool UHackComponent::IsOnCooldown(int32 Index) const
{
	return IsValidIndex(Index) && Runtime[Index].bOnCooldown;
}

float UHackComponent::GetRemainingCooldown(int32 Index) const
{
	if (!IsValidIndex(Index) || !GetWorld())
	{
		return 0.f;
	}

	if (!Runtime[Index].bOnCooldown)
	{
		return 0.f;
	}

	return FMath::Max(0.f, GetWorld()->GetTimerManager().GetTimerRemaining(Runtime[Index].CooldownTimer));
}

float UHackComponent::GetRemainingDuration(int32 Index) const
{
	if (!IsValidIndex(Index) || !GetWorld())
	{
		return 0.f;
	}

	if (!Runtime[Index].bActive)
	{
		return 0.f;
	}

	return FMath::Max(0.f, GetWorld()->GetTimerManager().GetTimerRemaining(Runtime[Index].DurationTimer));
}

// Lookups

ABossCharacter* UHackComponent::GetBoss()
{
	if (CachedBoss)
	{
		return CachedBoss;
	}

	// Resolved lazily: the boss may register after the player spawns.
	if (const ADeadSignalGameMode* GameMode = Cast<ADeadSignalGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		CachedBoss = Cast<ABossCharacter>(GameMode->GetBossActor());
	}

	if (!CachedBoss)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Hacks] No boss registered - boss-targeting hacks will do nothing."));
	}

	return CachedBoss;
}

UHealthComponent* UHackComponent::GetOwnerHealth() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UHealthComponent>() : nullptr;
}
