#include "UI/EncounterMessageWidget.h"

#include "Combat/BossCharacter.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UEncounterMessageWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GameMode = Cast<ADeadSignalGameMode>(UGameplayStatics::GetGameMode(this));
	if (!GameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Message] Game mode is not ADeadSignalGameMode - no messages will play."));
		return;
	}

	GameMode->OnEncounterStateChanged.AddDynamic(this, &UEncounterMessageWidget::HandleEncounterStateChanged);
	GameMode->OnBossRegistered.AddDynamic(this, &UEncounterMessageWidget::HandleBossRegistered);

	// The boss may have registered already or may arrive later - its BeginPlay
	// and this widget's construction have no guaranteed order. Cover both.
	BindToBoss(GameMode->GetBossActor());

	// Seeded, because the game mode may already have started the fight in its
	// own BeginPlay and the broadcast we would have heard is gone.
	HandleEncounterStateChanged(GameMode->GetEncounterState());
}

void UEncounterMessageWidget::NativeDestruct()
{
	if (GameMode)
	{
		GameMode->OnEncounterStateChanged.RemoveDynamic(this, &UEncounterMessageWidget::HandleEncounterStateChanged);
		GameMode->OnBossRegistered.RemoveDynamic(this, &UEncounterMessageWidget::HandleBossRegistered);
	}

	if (Boss)
	{
		Boss->OnPhaseChanged.RemoveDynamic(this, &UEncounterMessageWidget::HandlePhaseChanged);
	}

	// Otherwise a line cut off by a restart keeps talking over the reloaded level.
	StopVoice();

	Super::NativeDestruct();
}

void UEncounterMessageWidget::HandleEncounterStateChanged(EEncounterState NewState)
{
	// Whichever of Intro or Fighting arrives first, and only once.
	if (bIntroPlayed || (NewState != EEncounterState::Intro && NewState != EEncounterState::Fighting))
	{
		return;
	}

	bIntroPlayed = true;

	PlayLine(IntroLine);
}

void UEncounterMessageWidget::HandleBossRegistered(AActor* BossActor)
{
	BindToBoss(BossActor);
}

void UEncounterMessageWidget::BindToBoss(AActor* BossActor)
{
	// Guarded so the delegate firing after we already bound is harmless.
	if (Boss || !BossActor)
	{
		return;
	}

	Boss = Cast<ABossCharacter>(BossActor);
	if (!Boss)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Message] Registered boss %s is not an ABossCharacter - no phase messages."),
			*BossActor->GetName());
		return;
	}

	Boss->OnPhaseChanged.AddDynamic(this, &UEncounterMessageWidget::HandlePhaseChanged);

	UE_LOG(LogTemp, Log, TEXT("[Message] Bound to boss %s - phase messages are live."), *Boss->GetName());

	// Deliberately not seeded with the current phase, unlike the health bar and
	// the music director. Those describe a continuing state; this announces a
	// change, and the opening is the intro's to announce.
}

void UEncounterMessageWidget::HandlePhaseChanged(EBossPhase NewPhase)
{
	const FDialogueLine* Line = nullptr;

	switch (NewPhase)
	{
	case EBossPhase::Phase2:
		Line = &Phase2Line;
		break;

	case EBossPhase::Phase3:
		Line = &Phase3Line;
		break;

	default:
		break;
	}

	if (!Line || Line->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Message] Nothing to play for %s - its line has no text or voice."),
			*UEnum::GetValueAsString(NewPhase));
		return;
	}

	PlayLine(*Line);
}

void UEncounterMessageWidget::PlayLine(const FDialogueLine& Line)
{
	if (Line.IsEmpty())
	{
		return;
	}

	// The new line replaces the old one entirely - text and voice together - so
	// an interrupted voice cannot keep talking under different words.
	StopVoice();

	PlayMessage(Line.Text);

	if (!Line.Voice)
	{
		return;
	}

	ActiveVoice = UGameplayStatics::CreateSound2D(this, Line.Voice, 1.f, 1.f, 0.f,
		nullptr, false,true);

	if (ActiveVoice)
	{
		// CreateSound2D flags sounds as UI, which keeps them playing through a pause
		// The typing timer freezes on pause
		ActiveVoice->bIsUISound = false;
		ActiveVoice->Play();
	}
}

void UEncounterMessageWidget::StopVoice()
{
	if (ActiveVoice)
	{
		ActiveVoice->Stop();
		ActiveVoice = nullptr;
	}
}
