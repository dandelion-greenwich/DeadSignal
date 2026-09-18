#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Damageable.h"
#include "ProjectileBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/** Why a projectile stopped existing. Drives which FX the destroy hook plays. */
UENUM(BlueprintType)
enum class EProjectileEndReason : uint8
{
	ShotDown	UMETA(DisplayName = "Shot Down"),
	HitPlayer	UMETA(DisplayName = "Hit Player"),
	HitWorld	UMETA(DisplayName = "Hit World"),
	Expired		UMETA(DisplayName = "Expired")
};

/**
 * A boss projectile. Pooled, never spawned or destroyed during a fight.
 *
 * No UHealthComponent by design: it dies in one shot, so there is no health
 * value to track, and a bullet-hell keeps hundreds of these alive at once.
 * If you ever want a tanky projectile, add a hit counter here rather than a
 * whole component.
 */
UCLASS()
class BOSSFIGHTJAM_API AProjectileBase : public AActor, public IDamageable
{
	GENERATED_BODY()

public:
	AProjectileBase();

	/** Called by the pool. Places, arms and launches this projectile. */
	void Activate(const FTransform& SpawnTransform, AActor* InInstigator);

	/**
	 * Ends this projectile: runs the destroy hook, then hands it back to the
	 * pool. Safe to call twice - the second call is ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void Deactivate(EProjectileEndReason Reason, const FHitResult& Hit);

	UFUNCTION(BlueprintPure, Category = "Projectile")
	bool IsActive() const { return bIsActive; }

	/** Puts the projectile in its dormant state without touching the pool. */
	void EnterDormantState();

	// IDamageable
	virtual float ReceiveShot_Implementation(float Damage, AActor* DamageInstigator, const FHitResult& Hit) override;

protected:
	virtual void BeginPlay() override;

	/**
	 * Runs immediately BEFORE the projectile returns to the pool - explosion,
	 * decal, sound, screen shake. Override in BP_BossProjectile and branch on
	 * Reason; being shot down should not look like splashing on a wall.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Projectile")
	void OnProjectileDestroyed(EProjectileEndReason Reason, const FHitResult& Hit);

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void HandleBlockingHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0"))
	float Damage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0"))
	float Speed = 900.f;

	/** Safety net so a stray projectile cannot leak out of the pool forever. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.1"))
	float MaxLifeSeconds = 10.f;

private:
	void HandleLifeExpired();

	UPROPERTY()
	TObjectPtr<AActor> ProjectileInstigator;
	bool bIsActive = false;
	FTimerHandle LifeTimer;
};
