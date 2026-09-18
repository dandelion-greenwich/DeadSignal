#include "Debug/DebugControlLibrary.h"

#include "Combat/BossCharacter.h"
#include "Combat/HackComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/WeaponComponent.h"
#include "Combat/ProjectilePoolSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

// ---------------------------------------------------------------- Resolution

UWorld* UDebugControlLibrary::ResolveWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	// PIE first: in the editor there is also an editor world, and it has no
	// player or boss in it. Falling back to Game covers packaged dev builds.
	UWorld* Fallback = nullptr;

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (!Context.World())
		{
			continue;
		}

		if (Context.WorldType == EWorldType::PIE)
		{
			return Context.World();
		}

		if (Context.WorldType == EWorldType::Game && !Fallback)
		{
			Fallback = Context.World();
		}
	}

	return Fallback;
}

bool UDebugControlLibrary::IsGameRunning()
{
	return ResolveWorld() != nullptr;
}

APawn* UDebugControlLibrary::GetPlayerPawn()
{
	const UWorld* World = ResolveWorld();
	if (!World)
	{
		return nullptr;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	return PC ? PC->GetPawn() : nullptr;
}

UHealthComponent* UDebugControlLibrary::GetPlayerHealthComponent()
{
	const APawn* Pawn = GetPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UHealthComponent>() : nullptr;
}

UHackComponent* UDebugControlLibrary::GetHackComponent()
{
	const APawn* Pawn = GetPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UHackComponent>() : nullptr;
}

UWeaponComponent* UDebugControlLibrary::GetWeaponComponent()
{
	const APawn* Pawn = GetPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UWeaponComponent>() : nullptr;
}

ADeadSignalGameMode* UDebugControlLibrary::GetGameMode()
{
	const UWorld* World = ResolveWorld();
	return World ? Cast<ADeadSignalGameMode>(World->GetAuthGameMode()) : nullptr;
}

ABossCharacter* UDebugControlLibrary::GetBoss()
{
	const ADeadSignalGameMode* GameMode = GetGameMode();
	return GameMode ? Cast<ABossCharacter>(GameMode->GetBossActor()) : nullptr;
}

// ---------------------------------------------------------------- Player

float UDebugControlLibrary::GetPlayerHealth()
{
	const UHealthComponent* Health = GetPlayerHealthComponent();
	return Health ? Health->GetCurrentHealth() : 0.f;
}

float UDebugControlLibrary::GetPlayerMaxHealth()
{
	const UHealthComponent* Health = GetPlayerHealthComponent();
	return Health ? Health->GetMaxHealth() : 0.f;
}

bool UDebugControlLibrary::IsPlayerInvulnerable()
{
	const UHealthComponent* Health = GetPlayerHealthComponent();
	return Health && Health->IsInvulnerable();
}

bool UDebugControlLibrary::IsPlayerInHitImmunity()
{
	const UHealthComponent* Health = GetPlayerHealthComponent();
	return Health && Health->IsInHitImmunity();
}

int32 UDebugControlLibrary::GetPlayerAmmo()
{
	const UWeaponComponent* Weapon = GetWeaponComponent();
	return Weapon ? Weapon->GetCurrentAmmo() : 0;
}

int32 UDebugControlLibrary::GetPlayerMagazineSize()
{
	const UWeaponComponent* Weapon = GetWeaponComponent();
	return Weapon ? Weapon->GetMagazineSize() : 0;
}

void UDebugControlLibrary::DamagePlayer(float Amount)
{
	if (UHealthComponent* Health = GetPlayerHealthComponent())
	{
		Health->ApplyDamage(Amount);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug] No player health component."));
	}
}

void UDebugControlLibrary::KillPlayer()
{
	// SetHealth rather than ApplyDamage: a forced kill should not be stopped by
	// god mode or an i-frame window.
	if (UHealthComponent* Health = GetPlayerHealthComponent())
	{
		Health->SetHealth(0.f);
	}
}

void UDebugControlLibrary::HealPlayer(float Amount)
{
	if (UHealthComponent* Health = GetPlayerHealthComponent())
	{
		Health->Heal(Amount > 0.f ? Amount : Health->GetMaxHealth());
	}
}

void UDebugControlLibrary::SetPlayerGodMode(bool bEnabled)
{
	if (UHealthComponent* Health = GetPlayerHealthComponent())
	{
		Health->SetInvulnerable(bEnabled);
	}
}

void UDebugControlLibrary::RefillAmmo()
{
	if (UWeaponComponent* Weapon = GetWeaponComponent())
	{
		Weapon->Reload();
	}
}

// ---------------------------------------------------------------- Boss

bool UDebugControlLibrary::HasBoss()
{
	return GetBoss() != nullptr;
}

float UDebugControlLibrary::GetBossHealth()
{
	const ABossCharacter* Boss = GetBoss();
	const UHealthComponent* Health = Boss ? Boss->GetHealthComponent() : nullptr;
	return Health ? Health->GetCurrentHealth() : 0.f;
}

float UDebugControlLibrary::GetBossMaxHealth()
{
	const ABossCharacter* Boss = GetBoss();
	const UHealthComponent* Health = Boss ? Boss->GetHealthComponent() : nullptr;
	return Health ? Health->GetMaxHealth() : 0.f;
}

EBossPhase UDebugControlLibrary::GetBossPhase()
{
	const ABossCharacter* Boss = GetBoss();
	return Boss ? Boss->GetCurrentPhase() : EBossPhase::Phase1;
}

EShieldState UDebugControlLibrary::GetBossShieldState()
{
	const ABossCharacter* Boss = GetBoss();
	return Boss ? Boss->GetShieldState() : EShieldState::Up;
}

bool UDebugControlLibrary::IsBossStunned()
{
	const ABossCharacter* Boss = GetBoss();
	return Boss && Boss->IsStunned();
}

void UDebugControlLibrary::DamageBoss(float Amount)
{
	ABossCharacter* Boss = GetBoss();
	UHealthComponent* Health = Boss ? Boss->GetHealthComponent() : nullptr;

	if (Health)
	{
		Health->ApplyDamage(Amount);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug] No boss registered."));
	}
}

void UDebugControlLibrary::KillBoss()
{
	ABossCharacter* Boss = GetBoss();
	if (UHealthComponent* Health = Boss ? Boss->GetHealthComponent() : nullptr)
	{
		Health->SetHealth(0.f);
	}
}

void UDebugControlLibrary::SetBossHealth(float Value)
{
	ABossCharacter* Boss = GetBoss();
	if (UHealthComponent* Health = Boss ? Boss->GetHealthComponent() : nullptr)
	{
		Health->SetHealth(Value);
	}
}

void UDebugControlLibrary::SetBossPhase(EBossPhase Phase)
{
#if !UE_BUILD_SHIPPING
	if (ABossCharacter* Boss = GetBoss())
	{
		Boss->DebugSetPhase(Phase);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug] No boss registered."));
	}
#endif
}

void UDebugControlLibrary::DropBossShield(float Seconds)
{
	if (ABossCharacter* Boss = GetBoss())
	{
		Boss->DropShield(Seconds);
	}
}

void UDebugControlLibrary::StunBoss(float Seconds)
{
	if (ABossCharacter* Boss = GetBoss())
	{
		Boss->ApplyStun(Seconds);
	}
}

bool UDebugControlLibrary::IsBossTransitioning()
{
	const ABossCharacter* Boss = GetBoss();
	return Boss && Boss->IsTransitioning();
}

float UDebugControlLibrary::GetBossShieldRemaining()
{
	const ABossCharacter* Boss = GetBoss();
	return Boss ? Boss->GetRemainingShieldDownTime() : 0.f;
}

// ---------------------------------------------------------------- Boss attacks

bool UDebugControlLibrary::IsBossAttacking()
{
	const ABossCharacter* Boss = GetBoss();
	return Boss && Boss->IsAttacking();
}

int32 UDebugControlLibrary::GetBossStepNumber()
{
	const ABossCharacter* Boss = GetBoss();
	return Boss && Boss->IsAttacking() ? Boss->GetCurrentStepIndex() + 1 : 0;
}

int32 UDebugControlLibrary::GetBossSequenceLength()
{
	const ABossCharacter* Boss = GetBoss();
	return Boss ? Boss->GetCurrentSequenceLength() : 0;
}

FString UDebugControlLibrary::GetBossCurrentAttackName()
{
	const ABossCharacter* Boss = GetBoss();
	if (!Boss || !Boss->IsAttacking())
	{
		return TEXT("idle");
	}

	return UEnum::GetDisplayValueAsText(Boss->GetCurrentAttackType()).ToString();
}

// ---------------------------------------------------------------- Pool

int32 UDebugControlLibrary::GetPoolFreeCount()
{
	const UWorld* World = ResolveWorld();
	const UProjectilePoolSubsystem* Pool = World ? World->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	return Pool ? Pool->GetFreeCount() : 0;
}

int32 UDebugControlLibrary::GetPoolActiveCount()
{
	const UWorld* World = ResolveWorld();
	const UProjectilePoolSubsystem* Pool = World ? World->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	return Pool ? Pool->GetActiveCount() : 0;
}

int32 UDebugControlLibrary::GetPoolTotalCount()
{
	const UWorld* World = ResolveWorld();
	const UProjectilePoolSubsystem* Pool = World ? World->GetSubsystem<UProjectilePoolSubsystem>() : nullptr;
	return Pool ? Pool->GetTotalCount() : 0;
}

// ---------------------------------------------------------------- Hacks

int32 UDebugControlLibrary::GetHackCount()
{
	const UHackComponent* Hacks = GetHackComponent();
	return Hacks ? Hacks->GetHackCount() : 0;
}

FHackDebugInfo UDebugControlLibrary::GetHackInfo(int32 Index)
{
	FHackDebugInfo Info;

	const UHackComponent* Hacks = GetHackComponent();
	if (!Hacks || Index < 0 || Index >= Hacks->GetHackCount())
	{
		return Info;
	}

	const FHackDefinition Definition = Hacks->GetHackDefinition(Index);

	Info.DisplayName = Definition.DisplayName.IsEmpty()
		? UEnum::GetDisplayValueAsText(Definition.Type)
		: Definition.DisplayName;

	Info.KeySequence = Hacks->GetKeySequence(Index);

	if (Hacks->IsHackActive(Index))
	{
		Info.State = TEXT("Active");
		Info.RemainingSeconds = Hacks->GetRemainingDuration(Index);
	}
	else if (Hacks->IsOnCooldown(Index))
	{
		Info.State = TEXT("Cooling");
		Info.RemainingSeconds = Hacks->GetRemainingCooldown(Index);
	}
	else
	{
		Info.State = TEXT("Available");
	}

	return Info;
}

void UDebugControlLibrary::ResetHackCooldowns()
{
#if !UE_BUILD_SHIPPING
	if (UHackComponent* Hacks = GetHackComponent())
	{
		Hacks->DebugResetCooldowns();
	}
#endif
}

// ---------------------------------------------------------------- Encounter

EEncounterState UDebugControlLibrary::GetEncounterState()
{
	const ADeadSignalGameMode* GameMode = GetGameMode();
	return GameMode ? GameMode->GetEncounterState() : EEncounterState::Intro;
}

void UDebugControlLibrary::RestartEncounter()
{
	if (ADeadSignalGameMode* GameMode = GetGameMode())
	{
		GameMode->RestartEncounterLevel();
	}
}

void UDebugControlLibrary::StartEncounter()
{
	if (ADeadSignalGameMode* GameMode = GetGameMode())
	{
		GameMode->StartEncounter();
	}
}

FString UDebugControlLibrary::GetStatusSummary()
{
	if (!IsGameRunning())
	{
		return TEXT("[Debug] No game running.");
	}

	FString Result = FString::Printf(
		TEXT("Encounter %s | Player %.0f/%.0f (ammo %d/%d) | Boss %.0f/%.0f phase %d shield %s%s"),
		*UEnum::GetDisplayValueAsText(GetEncounterState()).ToString(),
		GetPlayerHealth(), GetPlayerMaxHealth(), GetPlayerAmmo(), GetPlayerMagazineSize(),
		GetBossHealth(), GetBossMaxHealth(),
		static_cast<int32>(GetBossPhase()) + 1,
		*UEnum::GetDisplayValueAsText(GetBossShieldState()).ToString(),
		IsBossStunned() ? TEXT(" STUNNED") : TEXT(""));

	for (int32 i = 0; i < GetHackCount(); ++i)
	{
		const FHackDebugInfo Info = GetHackInfo(i);
		Result += FString::Printf(TEXT("\n  %d %s: %s"), i + 1, *Info.DisplayName.ToString(), *Info.State);

		if (Info.RemainingSeconds > 0.f)
		{
			Result += FString::Printf(TEXT(" %.1fs"), Info.RemainingSeconds);
		}
	}

	return Result;
}
