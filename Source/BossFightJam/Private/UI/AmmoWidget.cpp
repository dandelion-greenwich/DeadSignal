#include "UI/AmmoWidget.h"

#include "Combat/WeaponComponent.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UAmmoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Ammo] No owning pawn - the counter will show nothing."));
		return;
	}

	WeaponComponent = Pawn->FindComponentByClass<UWeaponComponent>();

	if (!WeaponComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Ammo] %s has no UWeaponComponent."), *Pawn->GetName());
		return;
	}

	WeaponComponent->OnAmmoChanged.AddDynamic(this, &UAmmoWidget::HandleAmmoChanged);
	WeaponComponent->OnReloadStarted.AddDynamic(this, &UAmmoWidget::HandleReloadStarted);
	WeaponComponent->OnReloadFinished.AddDynamic(this, &UAmmoWidget::HandleReloadFinished);
	WeaponComponent->OnFiredEmpty.AddDynamic(this, &UAmmoWidget::HandleFiredEmpty);

	// The weapon broadcasts its starting count in BeginPlay, which may already
	// have happened - so seed the display rather than waiting for the next shot.
	HandleAmmoChanged(WeaponComponent->GetCurrentAmmo(), WeaponComponent->GetMagazineSize());
}

void UAmmoWidget::NativeDestruct()
{
	if (WeaponComponent)
	{
		WeaponComponent->OnAmmoChanged.RemoveDynamic(this, &UAmmoWidget::HandleAmmoChanged);
		WeaponComponent->OnReloadStarted.RemoveDynamic(this, &UAmmoWidget::HandleReloadStarted);
		WeaponComponent->OnReloadFinished.RemoveDynamic(this, &UAmmoWidget::HandleReloadFinished);
		WeaponComponent->OnFiredEmpty.RemoveDynamic(this, &UAmmoWidget::HandleFiredEmpty);
	}

	Super::NativeDestruct();
}

// ---------------------------------------------------------------- Handlers

void UAmmoWidget::HandleAmmoChanged(int32 CurrentAmmo, int32 MagazineSize)
{
	// The numbers are data, so C++ writes them. Anything about how they look
	// belongs in OnAmmoUpdated.
	if (AmmoText)
	{
		AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentAmmo, MagazineSize)));
	}

	OnAmmoUpdated(CurrentAmmo, MagazineSize);
}

void UAmmoWidget::HandleReloadStarted(float Duration)
{
	OnReloadBegan(Duration);
}

void UAmmoWidget::HandleReloadFinished()
{
	OnReloadEnded();
}

void UAmmoWidget::HandleFiredEmpty()
{
	OnFiredEmpty();
}

// ---------------------------------------------------------------- Queries

float UAmmoWidget::GetAmmoFraction() const
{
	if (!WeaponComponent)
	{
		return 0.f;
	}

	const int32 Max = WeaponComponent->GetMagazineSize();
	return Max > 0 ? static_cast<float>(WeaponComponent->GetCurrentAmmo()) / Max : 0.f;
}
