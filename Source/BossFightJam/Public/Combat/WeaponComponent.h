#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "WeaponComponent.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class UNiagaraSystem;
class UHackComponent;
class USoundCue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChanged, int32, CurrentAmmo, int32, MagazineSize);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReloadStarted, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReloadFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFiredEmpty);

UCLASS(ClassGroup = (DeadSignal), meta = (BlueprintSpawnableComponent))
class BOSSFIGHTJAM_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();
	
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool Fire();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool CanFire() const;

	// Seconds until the next shot is allowed. 0 when ready
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetRemainingCooldown() const;
	
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetMuzzleLocation() const;
	
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	bool Reload();

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	bool CanReload() const;

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	int32 GetMagazineSize() const { return MagazineSize; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	bool IsMagazineEmpty() const { return CurrentAmmo <= 0; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	bool IsReloading() const { return bIsReloading; }

	/** 0 to 1 through the current reload, for a progress bar. 0 when not reloading. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	float GetReloadProgress() const;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Ammo")
	FOnAmmoChanged OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Ammo")
	FOnReloadStarted OnReloadStarted;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Ammo")
	FOnReloadFinished OnReloadFinished;

	/** Broadcast alongside the OnFireFailedEmpty event, for listeners off the weapon. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Ammo")
	FOnFiredEmpty OnFiredEmpty;

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Setup",
		meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SkeletalMeshComponent"))
	FComponentReference GunMeshReference;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animation")
	TObjectPtr<UAnimSequence> FireSequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Animation")
	TObjectPtr<UAnimSequence> ReloadSequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|FX")
	TObjectPtr<UNiagaraSystem> MuzzleFlash;

	// Socket on the gun mesh that muzzle FX originate from.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName MuzzleSocketName = TEXT("MuzzleSocket");

	// Everything visual - muzzle flash, tracer, impact decal, recoil, sound.
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnFired(const FHitResult& Hit, bool bHitSomething, float DamageDealt);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnFireFailedEmpty();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Damage = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "1.0"))
	float Range = 15000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float FireInterval = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (ClampMin = "1"))
	int32 MagazineSize = 12;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (ClampMin = "0.0"))
	float ReloadDuration = 1.5f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	bool bAutoReloadWhenEmpty = false;

	/** Draws every shot in the world: green missed, red hit, sphere at impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
	bool bDrawDebugTrace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug", meta = (ClampMin = "0.0"))
	float DebugTraceDuration = 2.f;

private:
	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> GunMesh;

	UPROPERTY()
	TObjectPtr<UHackComponent> HackComponent;

	// Starts long enough ago that the very first shot always passes the rate gate
	float LastFireTime = -1000.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Weapon|Ammo", meta = (AllowPrivateAccess = "true"))
	int32 CurrentAmmo = 0;

	bool bIsReloading = false;

	UPROPERTY(EditAnywhere, Category = "Weapon|Ammo")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;
	
	UPROPERTY(EditAnywhere, Category = "Weapon|Audio")
	TObjectPtr<USoundCue> FireSound;
	
	UPROPERTY(EditAnywhere, Category = "Weapon|Audio")
	TObjectPtr<USoundCue> DryFireSound;

	UPROPERTY(EditAnywhere, Category = "Weapon|Audio")
	TObjectPtr<USoundBase> ReloadSound;

	FTimerHandle ReloadTimer;

	void FinishReload();
};
