#pragma once

#include "CoreMinimal.h"
#include "UI/TypewriterWidget.h"
#include "Core/DeadSignalGameMode.h"
#include "Core/DeadSignalTypes.h"
#include "EncounterMessageWidget.generated.h"

class ABossCharacter;
class UAudioComponent;
class USoundBase;

USTRUCT(BlueprintType)
struct FDialogueLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (MultiLine = true))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TObjectPtr<USoundBase> Voice;

	bool IsEmpty() const { return Text.IsEmpty() && !Voice; }
};

/**
 * A typewriter that announces the fight.
 *
 * The messages live here rather than on the game mode, which does not know
 * what phase the boss is in and should not learn. Nothing calls this - it
 * binds to the encounter and boss delegates the same way the music director
 * does, so there is no blueprint wiring to get wrong.
 */
UCLASS()
class BOSSFIGHTJAM_API UEncounterMessageWidget : public UTypewriterWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Played when the fight opens. */
	UPROPERTY(EditDefaultsOnly, Category = "Encounter Message")
	FDialogueLine IntroLine;

	UPROPERTY(EditDefaultsOnly, Category = "Encounter Message")
	FDialogueLine Phase2Line;

	UPROPERTY(EditDefaultsOnly, Category = "Encounter Message")
	FDialogueLine Phase3Line;

private:
	UFUNCTION()
	void HandleEncounterStateChanged(EEncounterState NewState);

	UFUNCTION()
	void HandleBossRegistered(AActor* BossActor);

	UFUNCTION()
	void HandlePhaseChanged(EBossPhase NewPhase);

	void BindToBoss(AActor* BossActor);

	/** Types the text and starts the voice, cutting off whatever line was playing. */
	void PlayLine(const FDialogueLine& Line);

	void StopVoice();

	UPROPERTY(Transient)
	TObjectPtr<ADeadSignalGameMode> GameMode;

	/** The voice currently speaking, kept so an interrupting line can stop it. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveVoice;

	UPROPERTY(Transient)
	TObjectPtr<ABossCharacter> Boss;

	/** The opening moment has passed, so Intro and Fighting cannot both fire it. */
	bool bIntroPlayed = false;
};
