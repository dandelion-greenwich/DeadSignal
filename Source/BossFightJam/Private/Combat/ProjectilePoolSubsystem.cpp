#include "Combat/ProjectilePoolSubsystem.h"

#include "Combat/ProjectileBase.h"
#include "Engine/World.h"

void UProjectilePoolSubsystem::SetProjectileClass(TSubclassOf<AProjectileBase> InClass)
{
	if (!InClass)
	{
		return;
	}

	if (ProjectileClass && ProjectileClass != InClass && TotalSpawned > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Pool] Projectile class changed after %d were pooled. The free list still holds the old class."),
			TotalSpawned);
	}

	ProjectileClass = InClass;
}

void UProjectilePoolSubsystem::Prewarm(TSubclassOf<AProjectileBase> InClass, int32 Count)
{
	SetProjectileClass(InClass);

	for (int32 i = 0; i < Count; ++i)
	{
		if (AProjectileBase* Projectile = SpawnNew())
		{
			Free.Add(Projectile);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Pool] Prewarmed %d projectiles."), Free.Num());
}

AProjectileBase* UProjectilePoolSubsystem::Acquire(const FTransform& SpawnTransform, AActor* Instigator)
{
	AProjectileBase* Projectile = nullptr;

	// Pop until we find a live one - an entry could have been garbage collected
	// if something destroyed it behind our back.
	while (Free.Num() > 0 && !Projectile)
	{
		Projectile = Free.Pop();
	}

	if (!Projectile)
	{
		// Pool ran dry. Growing beats dropping the shot, but a climbing
		// GetTotalCount means the prewarm figure is too low.
		Projectile = SpawnNew();
	}

	if (!Projectile)
	{
		return nullptr;
	}

	Projectile->Activate(SpawnTransform, Instigator);
	return Projectile;
}

void UProjectilePoolSubsystem::Release(AProjectileBase* Projectile)
{
	if (!Projectile)
	{
		return;
	}

	// Guards against a double release putting the same actor in twice, which
	// would later hand the same projectile to two callers.
	if (Free.Contains(Projectile))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Pool] %s released twice - ignoring."), *Projectile->GetName());
		return;
	}

	Free.Add(Projectile);
}

AProjectileBase* UProjectilePoolSubsystem::SpawnNew()
{
	UWorld* World = GetWorld();
	if (!World || !ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Pool] No projectile class set - call Prewarm or SetProjectileClass first."));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AProjectileBase* Projectile = World->SpawnActor<AProjectileBase>(ProjectileClass, FTransform::Identity, Params);
	if (Projectile)
	{
		++TotalSpawned;
	}

	return Projectile;
}

void UProjectilePoolSubsystem::Deinitialize()
{
	Free.Empty();
	TotalSpawned = 0;

	Super::Deinitialize();
}
