#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Damageable.generated.h"

UINTERFACE(Blueprintable, MinimalAPI)
class UDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything a shot can hurt.
 *
 * Two implementers with genuinely different behaviour: the boss routes through
 * its UHealthComponent, a projectile just dies. Blueprintable so destructible
 * cover and environment hazards can join later without C++.
 */
class BOSSFIGHTJAM_API IDamageable
{
	GENERATED_BODY()

public:
	/**
	 * Take a hit. Returns damage actually dealt - after any reduction, clamped
	 * to what was left - so callers can show an honest hit number.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damage")
	float ReceiveShot(float Damage, AActor* DamageInstigator, const FHitResult& Hit);
};
