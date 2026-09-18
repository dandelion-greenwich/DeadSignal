#include "Combat/WeaponComponent.h"

#include "Core/Damageable.h"
#include "Combat/HealthComponent.h"
#include "Combat/HackComponent.h"
#include "Core/DeadSignalGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// The camera belongs to the pawn, not to us. A first-person character has
	// exactly one, so finding it beats making someone wire up a reference.
	Camera = Owner->FindComponentByClass<UCameraComponent>();

	if (!Camera)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Weapon] No camera component on %s - Fire will do nothing."),
			*Owner->GetName());
	}

	// Resolved once here rather than per shot - the reference is a name lookup.
	GunMesh = Cast<USkeletalMeshComponent>(GunMeshReference.GetComponent(Owner));

	HackComponent = Owner->FindComponentByClass<UHackComponent>();

	CurrentAmmo = MagazineSize;
	OnAmmoChanged.Broadcast(CurrentAmmo, MagazineSize);
}

FVector UWeaponComponent::GetMuzzleLocation() const
{
	if (GunMesh && GunMesh->DoesSocketExist(MuzzleSocketName))
	{
		return GunMesh->GetSocketLocation(MuzzleSocketName);
	}

	return Camera ? Camera->GetComponentLocation() : FVector::ZeroVector;
}

bool UWeaponComponent::CanFire() const
{
	if (!Camera || !GetWorld())
	{
		return false;
	}

	if (bIsReloading)
	{
		return false;
	}

	// The same gate the hack component uses, so pause and the end of the fight
	// stop shooting without the Blueprint having to check anything.
	if (const ADeadSignalGameMode* GameMode = Cast<ADeadSignalGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		if (!GameMode->IsFighting())
		{
			return false;
		}
	}

	// Either shoot or hack, player can't do both
	if (HackComponent && HackComponent->IsPanelOpen())
	{
		return false;
	}

	// A dead player should not keep shooting.
	if (const AActor* Owner = GetOwner())
	{
		if (const UHealthComponent* Health = Owner->FindComponentByClass<UHealthComponent>())
		{
			if (Health->IsDead())
			{
				return false;
			}
		}
	}

	return GetWorld()->GetTimeSeconds() - LastFireTime >= FireInterval;
}

float UWeaponComponent::GetRemainingCooldown() const
{
	if (!GetWorld())
	{
		return 0.f;
	}

	const float Elapsed = GetWorld()->GetTimeSeconds() - LastFireTime;
	return FMath::Max(0.f, FireInterval - Elapsed);
}

bool UWeaponComponent::Fire()
{
	if (!CanFire())
	{
		UGameplayStatics::PlaySound2D(this, DryFireSound, 1.f, 1.f, 0.f,
	 nullptr,nullptr, false);
		return false;
	}

	// Stamped before the empty check so holding the trigger on an empty
	// magazine dry-clicks at the fire rate rather than once per frame.
	LastFireTime = GetWorld()->GetTimeSeconds();

	if (CurrentAmmo <= 0)
	{
		OnFireFailedEmpty();
		OnFiredEmpty.Broadcast();

		if (bAutoReloadWhenEmpty)
		{
			Reload();
		}

		return false;
	}

	--CurrentAmmo;
	OnAmmoChanged.Broadcast(CurrentAmmo, MagazineSize);
	
	UGameplayStatics::PlaySound2D(this, FireSound, 1.f, 1.f, 0.f,
 		nullptr,nullptr, false);

	if (GunMesh && FireSequence)
	{
		GunMesh->PlayAnimation(FireSequence, false);
	}

	if (GunMesh && MuzzleFlash)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(MuzzleFlash, GunMesh, MuzzleSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget,
			false, true, ENCPoolMethod::AutoRelease);
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraShake(CameraShakeClass, 1.0f);
	}

	// From the camera, not the muzzle - what the crosshair covers is what gets hit.
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * Range;
	
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponFire), false);
	Params.AddIgnoredActor(GetOwner());

	// LineTraceSingle returns the first blocking hit, which is exactly the
	// "damage the first object" rule - no sorting needed.
	FHitResult Hit;
	const bool bHitSomething = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugTrace)
	{
		// Stops at the impact point, so the line shows what actually blocked the
		// shot - useful when a projectile body-blocks one meant for the boss.
		const FVector TraceEnd = bHitSomething ? Hit.ImpactPoint : End;
		const FColor LineColour = bHitSomething ? FColor::Red : FColor::Green;

		DrawDebugLine(GetWorld(), Start, TraceEnd, LineColour, false, DebugTraceDuration, 0, 1.f);

		if (bHitSomething)
		{
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 12.f, 12, FColor::Yellow, false, DebugTraceDuration);
			UE_LOG(LogTemp, Log, TEXT("[Weapon] Hit %s (component %s)"),
				*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()));
		}
	}
#endif

	float DamageDealt = 0.f;

	if (bHitSomething)
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			if (HitActor->Implements<UDamageable>())
			{
				DamageDealt = IDamageable::Execute_ReceiveShot(HitActor, Damage, GetOwner(), Hit);
			}
		}
	}

	OnFired(Hit, bHitSomething, DamageDealt);

	return true;
}

// ---------------------------------------------------------------- Ammo

bool UWeaponComponent::CanReload() const
{
	return !bIsReloading && CurrentAmmo < MagazineSize;
}

bool UWeaponComponent::Reload()
{
	if (!CanReload() || !GetWorld())
	{
		return false;
	}

	bIsReloading = true;

	if (GunMesh && ReloadSequence)
	{
		GunMesh->PlayAnimation(ReloadSequence, /*bLooping=*/false);
	}

	if (ReloadSound)
	{
		UGameplayStatics::PlaySound2D(this, ReloadSound, 1.f, 1.f, 0.f,
nullptr,nullptr, false);
	}

	OnReloadStarted.Broadcast(ReloadDuration);

	// A zero duration would never fire a timer, so finish immediately instead.
	if (ReloadDuration <= 0.f)
	{
		FinishReload();
		return true;
	}

	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this,
		&UWeaponComponent::FinishReload, ReloadDuration, false);

	return true;
}

void UWeaponComponent::FinishReload()
{
	bIsReloading = false;

	// Always a full magazine - there is no reserve to run dry.
	CurrentAmmo = MagazineSize;

	OnAmmoChanged.Broadcast(CurrentAmmo, MagazineSize);
	OnReloadFinished.Broadcast();
}

float UWeaponComponent::GetReloadProgress() const
{
	if (!bIsReloading || ReloadDuration <= 0.f || !GetWorld())
	{
		return 0.f;
	}

	const float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(ReloadTimer);
	return FMath::Clamp(1.f - (Remaining / ReloadDuration), 0.f, 1.f);
}
