#include "UI/BossHealthBarWidget.h"

#include "Combat/BossCharacter.h"
#include "Core/DeadSignalGameMode.h"
#include "Combat/HealthComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

void UBossHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ADeadSignalGameMode* GameMode = Cast<ADeadSignalGameMode>(UGameplayStatics::GetGameMode(this));
	if (!GameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossBar] Game mode is not ADeadSignalGameMode - the bar will show nothing."));
		return;
	}

	// The boss may already be registered, or may arrive later - its BeginPlay
	// and the HUD's construction have no guaranteed order. Cover both.
	GameMode->OnBossRegistered.AddDynamic(this, &UBossHealthBarWidget::HandleBossRegistered);
	BindToBoss(GameMode->GetBossActor());
}

void UBossHealthBarWidget::NativeDestruct()
{
	if (ADeadSignalGameMode* GameMode = Cast<ADeadSignalGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GameMode->OnBossRegistered.RemoveDynamic(this, &UBossHealthBarWidget::HandleBossRegistered);
	}

	if (BossHealth)
	{
		BossHealth->OnHealthChanged.RemoveDynamic(this, &UBossHealthBarWidget::HandleHealthChanged);
		BossHealth->OnDeath.RemoveDynamic(this, &UBossHealthBarWidget::HandleDeath);
	}

	if (Boss)
	{
		Boss->OnPhaseChanged.RemoveDynamic(this, &UBossHealthBarWidget::HandlePhaseChanged);
		Boss->OnShieldStateChanged.RemoveDynamic(this, &UBossHealthBarWidget::HandleShieldStateChanged);
	}

	Super::NativeDestruct();
}

void UBossHealthBarWidget::HandleBossRegistered(AActor* BossActor)
{
	BindToBoss(BossActor);
}

void UBossHealthBarWidget::BindToBoss(AActor* BossActor)
{
	// Guarded so the delegate firing after we already bound is harmless.
	if (Boss || !BossActor)
	{
		return;
	}

	Boss = Cast<ABossCharacter>(BossActor);
	if (!Boss)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossBar] Registered boss %s is not an ABossCharacter."), *BossActor->GetName());
		return;
	}

	BossHealth = Boss->GetHealthComponent();
	if (!BossHealth)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossBar] Boss has no UHealthComponent."));
		Boss = nullptr;
		return;
	}

	BossHealth->OnHealthChanged.AddDynamic(this, &UBossHealthBarWidget::HandleHealthChanged);
	BossHealth->OnDeath.AddDynamic(this, &UBossHealthBarWidget::HandleDeath);

	Boss->OnPhaseChanged.AddDynamic(this, &UBossHealthBarWidget::HandlePhaseChanged);
	Boss->OnShieldStateChanged.AddDynamic(this, &UBossHealthBarWidget::HandleShieldStateChanged);

	// Seed every readout: the boss broadcast its starting values in BeginPlay,
	// which has already happened by the time we get here.
	HandleHealthChanged(BossHealth->GetCurrentHealth(), BossHealth->GetMaxHealth());
	HandlePhaseChanged(Boss->GetCurrentPhase());
	HandleShieldStateChanged(Boss->GetShieldState());

	OnBossFound();
}

void UBossHealthBarWidget::HandleHealthChanged(float CurrentHealth, float MaxHealth)
{
	const float Percent = MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f;

	if (HealthBar)
	{
		HealthBar->SetPercent(Percent);
	}

	if (HealthText)
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"),
			FMath::CeilToInt(CurrentHealth), FMath::CeilToInt(MaxHealth))));
	}

	OnHealthUpdated(CurrentHealth, MaxHealth, Percent);
}

void UBossHealthBarWidget::HandlePhaseChanged(EBossPhase NewPhase)
{
	if (PhaseText)
	{
		PhaseText->SetText(FText::FromString(FString::Printf(TEXT("Phase %d"),
			static_cast<int32>(NewPhase) + 1)));
	}

	OnPhaseChanged(NewPhase);
}

void UBossHealthBarWidget::HandleShieldStateChanged(EShieldState NewState)
{
	OnShieldStateChanged(NewState);
}

void UBossHealthBarWidget::HandleDeath()
{
	OnBossDied();
}
