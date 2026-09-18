#include "Combat/ProjectileBase.h"

#include "Combat/HealthComponent.h"
#include "Combat/ProjectilePoolSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

AProjectileBase::AProjectileBase()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(16.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Block Visibility so the player can shoot it down, block WorldStatic so it
	// dies on walls, overlap Pawn so it can damage the player, and ignore other
	// WorldDynamic so projectiles pass through each other.
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bAutoActivate = false;
}

void AProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	Collision->OnComponentBeginOverlap.AddDynamic(this, &AProjectileBase::HandleOverlap);
	Collision->OnComponentHit.AddDynamic(this, &AProjectileBase::HandleBlockingHit);

	// Spawned straight into the pool - dormant until acquired.
	EnterDormantState();
}

// ---------------------------------------------------------------- Lifecycle

void AProjectileBase::Activate(const FTransform& SpawnTransform, AActor* InInstigator)
{
	ProjectileInstigator = InInstigator;
	bIsActive = true;

	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::ResetPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	// Everything the projectile mutates in flight is restored here, not on
	// release - stale velocity from a previous life is the classic pooling bug.
	if (Movement)
	{
		Movement->SetUpdatedComponent(Collision);
		Movement->InitialSpeed = Speed;
		Movement->MaxSpeed = Speed;
		Movement->Velocity = SpawnTransform.GetRotation().GetForwardVector() * Speed;
		Movement->Activate(true);
		Movement->SetComponentTickEnabled(true);
	}

	GetWorldTimerManager().SetTimer(LifeTimer, this, &AProjectileBase::HandleLifeExpired,
		FMath::Max(MaxLifeSeconds, 0.1f), false);
}

void AProjectileBase::Deactivate(EProjectileEndReason Reason, const FHitResult& Hit)
{
	// An overlap and a blocking hit can both land in the same frame; without
	// this guard the projectile would be released into the pool twice.
	if (!bIsActive)
	{
		return;
	}
	bIsActive = false;

	GetWorldTimerManager().ClearTimer(LifeTimer);

	// The destroy hook runs while the projectile is still in place, so FX can
	// use its transform. Only then does it go back to the pool.
	OnProjectileDestroyed(Reason, Hit);

	EnterDormantState();

	if (UWorld* World = GetWorld())
	{
		if (UProjectilePoolSubsystem* Pool = World->GetSubsystem<UProjectilePoolSubsystem>())
		{
			Pool->Release(this);
		}
	}
}

void AProjectileBase::EnterDormantState()
{
	bIsActive = false;

	if (Movement)
	{
		Movement->StopMovementImmediately();
		Movement->Deactivate();
		Movement->SetComponentTickEnabled(false);
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void AProjectileBase::HandleLifeExpired()
{
	Deactivate(EProjectileEndReason::Expired, FHitResult());
}

void AProjectileBase::OnProjectileDestroyed_Implementation(EProjectileEndReason Reason, const FHitResult& Hit)
{
	// Empty for now
}

// ---------------------------------------------------------------- Collisions

void AProjectileBase::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult& Sweep)
{
	if (!bIsActive || !OtherActor || OtherActor == this || OtherActor == ProjectileInstigator)
	{
		return;
	}

	if (UHealthComponent* Health = OtherActor->FindComponentByClass<UHealthComponent>())
	{
		Health->ApplyDamage(Damage, ProjectileInstigator);
	}

	Deactivate(EProjectileEndReason::HitPlayer, Sweep);
}

void AProjectileBase::HandleBlockingHit(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (!bIsActive || OtherActor == ProjectileInstigator)
	{
		return;
	}

	Deactivate(EProjectileEndReason::HitWorld, Hit);
}

// ---------------------------------------------------------------- IDamageable

float AProjectileBase::ReceiveShot_Implementation(float InDamage, AActor* DamageInstigator, const FHitResult& Hit)
{
	if (!bIsActive)
	{
		return 0.f;
	}

	// One shot, here is no health to subtract from
	Deactivate(EProjectileEndReason::ShotDown, Hit);

	return InDamage;
}
