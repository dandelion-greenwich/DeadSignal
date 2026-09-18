#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/DeadSignalGameMode.h"
#include "EndScreenWidget.generated.h"

/**
 * Shared behaviour for the victory and defeat screens.
 *
 * Listens for OnGameEnded rather than OnEncounterStateChanged, so it appears
 * after the end-of-game slow motion rather than the instant the fight is
 * decided. Each subclass declares which outcome it answers to, so both can sit
 * in the viewport at once and only the right one shows.
 */
UCLASS(Abstract)
class BOSSFIGHTJAM_API UEndScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Reloads the level. Wire the Retry button to this. */
	UFUNCTION(BlueprintCallable, Category = "End Screen")
	void RestartEncounter();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Which outcome this screen answers to. */
	virtual EEncounterState GetTriggerState() const PURE_VIRTUAL(UEndScreenWidget::GetTriggerState, return EEncounterState::Victory;);

	/** The fight ended this way. Show the screen, play its animation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "End Screen")
	void OnShown();

	/** Show the cursor and switch to UI input when this screen appears. */
	UPROPERTY(EditDefaultsOnly, Category = "End Screen")
	bool bManageInputMode = true;

	/** Collapse until the matching outcome, then show. */
	UPROPERTY(EditDefaultsOnly, Category = "End Screen")
	bool bManageVisibility = true;

private:
	UFUNCTION()
	void HandleGameEnded(EEncounterState FinalState);

	ADeadSignalGameMode* GetGameMode() const;
};
