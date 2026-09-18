#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/DeadSignalTypes.h"
#include "HackPanelWidget.generated.h"

class UHackComponent;
class UPanelWidget;
class UHackRowWidget;
class UHackKeyWidget;

/** What one row of the panel should look like right now. */
UENUM(BlueprintType)
enum class EHackRowState : uint8
{
	/** Idle and ready. Nothing typed yet, or the input was just cleared. */
	Available	UMETA(DisplayName = "Available"),

	/** Still matches what has been typed - highlight it. */
	Matching	UMETA(DisplayName = "Matching"),

	/** No longer matches the typed prefix - grey it out. */
	Filtered	UMETA(DisplayName = "Filtered"),

	/** Its effect is running. */
	Active		UMETA(DisplayName = "Active"),

	/** Used recently, still cooling down. */
	Cooling		UMETA(DisplayName = "Cooling")
};

UCLASS(Abstract)
class BOSSFIGHTJAM_API UHackPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * The single question a row widget needs to ask. Order matters: a hack that
	 * is active or cooling reports that regardless of what has been typed,
	 * because the player cannot start it again either way.
	 */
	UFUNCTION(BlueprintPure, Category = "Hacks|Panel")
	EHackRowState GetRowState(int32 Index) const;

	/** How many of this row's keys the player has entered correctly so far. */
	UFUNCTION(BlueprintPure, Category = "Hacks|Panel")
	int32 GetHighlightedKeyCount(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Hacks|Panel")
	int32 GetHackCount() const;

	UFUNCTION(BlueprintPure, Category = "Hacks|Panel")
	FHackDefinition GetHackDefinition(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Hacks|Panel")
	TArray<int32> GetKeySequence(int32 Index) const;

	/** 0 to 1 through the cooldown. Poll this for a smooth ring or bar. */
	UFUNCTION(BlueprintPure, Category = "Hacks|Panel")
	float GetCooldownProgress(int32 Index) const;

	/** 0 to 1 through the active duration. Poll this for a depleting bar. */
	UFUNCTION(BlueprintPure, Category = "Hacks|Panel")
	float GetActiveProgress(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Hacks|Panel")
	UHackComponent* GetHackComponent() const { return HackComponent; }

	UFUNCTION(BlueprintPure, Category = "Hacks|Panel")
	bool IsPanelOpen() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativePreConstruct() override;

	/** Required - rows are spawned into this. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Hacks|Panel")
	TObjectPtr<UPanelWidget> RowContainer;

	UPROPERTY(EditDefaultsOnly, Category = "Hacks|Panel")
	TSubclassOf<UHackRowWidget> RowWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Hacks|Panel")
	TSubclassOf<UHackKeyWidget> KeyWidgetClass;

	/**
	 * Placeholder rows drawn in the UMG designer only. Rows are spawned at
	 * runtime, so without these the container would be an empty box and there
	 * would be nothing to lay out against.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Hacks|Panel|Preview", meta = (ClampMin = "0", ClampMax = "8"))
	int32 PreviewRowCount = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Hacks|Panel|Preview", meta = (ClampMin = "1", ClampMax = "8"))
	int32 PreviewSequenceLength = 5;


	/**
	 * Rebuild the rows here. Called once the hack component is bound, and again
	 * whenever the sequences are regenerated.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hacks|Panel")
	void OnPanelReady();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hacks|Panel")
	void OnPanelOpenChanged(bool bIsOpen);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hacks|Panel")
	void OnPatternsChanged();

	/** A key was accepted, or the input was cleared. Refresh every row. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hacks|Panel")
	void OnInputChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hacks|Panel")
	void OnHackFailed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hacks|Panel")
	void OnHackActivated(EHackType Type, int32 Index);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hacks|Panel")
	void OnHackExpired(EHackType Type, int32 Index);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hacks|Panel")
	void OnCooldownFinished(EHackType Type, int32 Index);

private:
	/** Finds the hack component on the owning pawn and binds every delegate. */
	void BindToHackComponent();

	/** Clears and respawns every row. Runs on bind, and again on regeneration. */
	void BuildRows();

	/**
	 * Pushes current state into every row. Wired to every event that can change
	 * one, so there is no branch left to forget.
	 */
	void RefreshAllRows();

	void BuildPreviewRows();

	UFUNCTION()
	void HandlePanelToggled(bool bIsOpen);

	UFUNCTION()
	void HandlePatternsRegenerated();

	UFUNCTION()
	void HandleInputChanged();

	UFUNCTION()
	void HandleHackFailed();

	UFUNCTION()
	void HandleHackActivated(EHackType Type, int32 Index);

	UFUNCTION()
	void HandleHackExpired(EHackType Type, int32 Index);

	UFUNCTION()
	void HandleCooldownFinished(EHackType Type, int32 Index);

	UPROPERTY()
	TObjectPtr<UHackComponent> HackComponent;

	UPROPERTY()
	TArray<TObjectPtr<UHackRowWidget>> RowWidgets;
};
