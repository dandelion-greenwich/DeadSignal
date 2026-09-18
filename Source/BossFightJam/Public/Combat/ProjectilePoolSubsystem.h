#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProjectilePoolSubsystem.generated.h"

class AProjectileBase;

/**
 * Recycles boss projectiles so a bullet-hell never spawns or destroys actors
 * mid-fight.
 *
 * A WorldSubsystem rather than a placed actor: it exists automatically in every
 * world, so there is nothing to drop in the level and nothing to forget.
 *
 */
UCLASS()
class BOSSFIGHTJAM_API UProjectilePoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Must be called once before Acquire. Prewarm does this for you. */
	UFUNCTION(BlueprintCallable, Category = "Projectile Pool")
	void SetProjectileClass(TSubclassOf<AProjectileBase> InClass);

	/**
	 * Spawns Count dormant projectiles up front. Call at fight start so the
	 * first volley does not hitch.
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile Pool")
	void Prewarm(TSubclassOf<AProjectileBase> InClass, int32 Count);

	/**
	 * Takes a projectile from the free list, or spawns one if empty, and
	 * launches it along SpawnTransform's forward vector.
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile Pool")
	AProjectileBase* Acquire(const FTransform& SpawnTransform, AActor* Instigator);

	/** Called by the projectile itself once its destroy hook has run. */
	void Release(AProjectileBase* Projectile);

	/** Dormant projectiles ready for reuse. */
	UFUNCTION(BlueprintPure, Category = "Projectile Pool")
	int32 GetFreeCount() const { return Free.Num(); }

	/** Projectiles currently in flight. */
	UFUNCTION(BlueprintPure, Category = "Projectile Pool")
	int32 GetActiveCount() const { return TotalSpawned - Free.Num(); }

	/** Every projectile this pool has ever created. Should plateau after prewarm. */
	UFUNCTION(BlueprintPure, Category = "Projectile Pool")
	int32 GetTotalCount() const { return TotalSpawned; }

	virtual void Deinitialize() override;

private:
	AProjectileBase* SpawnNew();

	UPROPERTY()
	TSubclassOf<AProjectileBase> ProjectileClass;

	UPROPERTY()
	TArray<TObjectPtr<AProjectileBase>> Free;

	int32 TotalSpawned = 0;
};
