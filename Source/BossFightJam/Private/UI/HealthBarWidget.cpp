#include "UI/HealthBarWidget.h"

#include "Combat/HealthComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HealthBar] No owning pawn - the bar will show nothing."));
		return;
	}

	HealthComponent = Pawn->FindComponentByClass<UHealthComponent>();

	if (!HealthComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HealthBar] %s has no UHealthComponent."), *Pawn->GetName());
		return;
	}

	HealthComponent->OnHealthChanged.AddDynamic(this, &UHealthBarWidget::HandleHealthChanged);
	HealthComponent->OnDamaged.AddDynamic(this, &UHealthBarWidget::HandleDamaged);
	HealthComponent->OnDeath.AddDynamic(this, &UHealthBarWidget::HandleDeath);

	// The component broadcasts its starting value in BeginPlay, which may
	// already have happened - so seed the bar rather than waiting for the first
	// hit to reveal it.
	HandleHealthChanged(HealthComponent->GetCurrentHealth(), HealthComponent->GetMaxHealth());
}

void UHealthBarWidget::NativeDestruct()
{
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &UHealthBarWidget::HandleHealthChanged);
		HealthComponent->OnDamaged.RemoveDynamic(this, &UHealthBarWidget::HandleDamaged);
		HealthComponent->OnDeath.RemoveDynamic(this, &UHealthBarWidget::HandleDeath);
	}

	Super::NativeDestruct();
}

void UHealthBarWidget::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
	const float Percent = MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f;

	if (HealthBar)
	{
		HealthBar->SetPercent(Percent);
	}

	// The numbers are data, so C++ writes them. How they look is not.
	if (HealthText)
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"),
			FMath::CeilToInt(CurrentHealth), FMath::CeilToInt(MaxHealth))));
	}

	OnHealthUpdated(CurrentHealth, MaxHealth, Percent);
}

void UHealthBarWidget::HandleDamaged(float AmountApplied, AActor* DamageInstigator)
{
	OnDamageTaken(AmountApplied, DamageInstigator);
}

void UHealthBarWidget::HandleDeath()
{
	OnDied();
}
