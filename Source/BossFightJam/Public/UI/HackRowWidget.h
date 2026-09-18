#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/DeadSignalTypes.h"
#include "UI/HackPanelWidget.h"
#include "HackRowWidget.generated.h"

class UPanelWidget;
class UTextBlock;
class UHackKeyWidget;

/**
 * One hack's row in the panel.
 *
 * Never queries the hack component. Everything it needs arrives as parameters,
 * so a row cannot show stale state - there is no path by which it refreshes
 * itself out of step with the others.
 */
UCLASS(Abstract)
class BOSSFIGHTJAM_API UHackRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Fills in the name and rebuilds the key glyphs. Called when the panel is
	 * first built and again whenever the sequences are regenerated.
	 */
	void SetupRow(int32 InRowIndex, const FHackDefinition& Definition,
		const TArray<int32>& Keys, TSubclassOf<UHackKeyWidget> InKeyWidgetClass);

	/** Pushes current state down to the keys, then hands styling to Blueprint. */
	void Refresh(EHackRowState State, int32 HighlightedKeys, float CooldownProgress, float ActiveProgress);

	UFUNCTION(BlueprintPure, Category = "Hacks|Row")
	int32 GetRowIndex() const { return RowIndex; }

	UFUNCTION(BlueprintPure, Category = "Hacks|Row")
	EHackRowState GetState() const { return CurrentState; }

protected:
	/**
	 * Switch on State here and apply colours. Filtered is the "typed something
	 * that does not match this row" case - grey the whole row from here.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hacks|Row")
	void OnRowRefreshed(EHackRowState State, float CooldownProgress, float ActiveProgress);

	/** Fires after the key glyphs are rebuilt, in case the layout needs adjusting. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hacks|Row")
	void OnRowSetup(const FHackDefinition& Definition);

	/** Required - the keys are spawned into this. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Hacks|Row")
	TObjectPtr<UPanelWidget> KeyContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Hacks|Row")
	TObjectPtr<UTextBlock> NameText;

private:
	UPROPERTY()
	TArray<TObjectPtr<UHackKeyWidget>> KeyWidgets;

	UPROPERTY()
	TSubclassOf<UHackKeyWidget> KeyWidgetClass;

	int32 RowIndex = 0;
	EHackRowState CurrentState = EHackRowState::Available;
};
