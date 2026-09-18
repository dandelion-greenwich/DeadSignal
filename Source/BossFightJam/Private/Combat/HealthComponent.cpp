#include "Combat/HealthComponent.h"

#include "Kismet/GameplayStatics.h"

UHealthComponent::UHealthComponent()
{
	// Nothing here needs to tick - everything is event driven.
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	bIsDead = false;

	// Fire once so any UI bound before BeginPlay starts with the right value.
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

float UHealthComponent::ApplyDamage(float Amount, AActor* DamageInstigator)
{
	// Two independent shields, either of which rejects the hit: the deliberate
	// 5s ability, and the brief post-hit grace period.
	if (bIsDead || bIsInvulnerable || IsInHitImmunity() || Amount <= 0.f)
	{
		return 0.f;
	}

	const float Scaled = Amount * DamageMultiplier;
	if (Scaled <= 0.f)
	{
		return 0.f;
	}

	// Never subtract more than is left, so the reported figure matches reality.
	const float Applied = FMath::Min(Scaled, CurrentHealth);
	CurrentHealth -= Applied;

	// Immunity starts the moment a hit actually lands, so OnDamaged doubles as
	// the "start the hit flash" signal - no extra delegate needed.
	if (HitImmunityDuration > 0.f)
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(HitImmunityTimer, HitImmunityDuration, false);
		}
	}

	OnDamaged.Broadcast(Applied, DamageInstigator);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.f)
	{
		bIsDead = true;
		OnDeath.Broadcast();
	}

	return Applied;
}

float UHealthComponent::Heal(float Amount)
{
	if (bIsDead || Amount <= 0.f)
	{
		return 0.f;
	}

	const float Restored = FMath::Min(Amount, MaxHealth - CurrentHealth);
	if (Restored <= 0.f)
	{
		return 0.f;
	}

	if (HealSound)
	{
		UGameplayStatics::PlaySound2D(this, HealSound, 1.f, 1.f, 0.f,
nullptr,nullptr, false);	
	}
	CurrentHealth += Restored;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	return Restored;
}

void UHealthComponent::SetHealth(float NewHealth)
{
	const float Clamped = FMath::Clamp(NewHealth, 0.f, MaxHealth);
	if (FMath::IsNearlyEqual(Clamped, CurrentHealth))
	{
		return;
	}

	CurrentHealth = Clamped;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.f && !bIsDead)
	{
		bIsDead = true;
		OnDeath.Broadcast();
	}
}

void UHealthComponent::ResetHealth()
{
	CurrentHealth = MaxHealth;
	bIsDead = false;

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitImmunityTimer);
	}

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

bool UHealthComponent::IsInHitImmunity() const
{
	return GetRemainingHitImmunity() > 0.f;
}

float UHealthComponent::GetRemainingHitImmunity() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}

	return FMath::Max(0.f, World->GetTimerManager().GetTimerRemaining(HitImmunityTimer));
}

void UHealthComponent::SetDamageMultiplier(float NewMultiplier)
{
	DamageMultiplier = FMath::Max(0.f, NewMultiplier);
}
