#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Core/DeadSignalTypes.h"
#include "HackComponent.generated.h"

class ABossCharacter;
class UHealthComponent;
class UMaterialInterface;
class UMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPanelToggled, bool, bIsOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPatternsRegenerated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInputChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHackFailed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHackActivated, EHackType, Type, int32, Index);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHackExpired, EHackType, Type, int32, Index);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCooldownFinished, EHackType, Type, int32, Index);

USTRUCT()
struct FHackRuntimeState
{
	GENERATED_BODY()

	/** Regenerated every panel open. Values are 1-4. */
	UPROPERTY()
	TArray<int32> KeySequence;

	bool bActive = false;
	
	bool bOnCooldown = false;

	FTimerHandle DurationTimer;
	FTimerHandle CooldownTimer;
};

UCLASS(ClassGroup = (DeadSignal), meta = (BlueprintSpawnableComponent))
class BOSSFIGHTJAM_API UHackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHackComponent();

	UFUNCTION(BlueprintCallable, Category = "Hacks")
	void TogglePanel();

	UFUNCTION(BlueprintCallable, Category = "Hacks")
	void OpenPanel();

	UFUNCTION(BlueprintCallable, Category = "Hacks")
	void ClosePanel();

	/** Feed one key press, 1-4. Ignored unless the panel is open and unlocked. */
	UFUNCTION(BlueprintCallable, Category = "Hacks")
	void SubmitKey(int32 Key);

	/** Locks out hacking entirely and closes the panel. For phase 3's hack invasion. */
	UFUNCTION(BlueprintCallable, Category = "Hacks")
	void SetInputLocked(bool bNewLocked);

	UFUNCTION(BlueprintPure, Category = "Hacks")
	bool IsPanelOpen() const { return bPanelOpen; }

	UFUNCTION(BlueprintPure, Category = "Hacks")
	bool IsInputLocked() const { return bInputLocked; }

	UFUNCTION(BlueprintPure, Category = "Hacks")
	int32 GetHackCount() const { return Hacks.Num(); }

	UFUNCTION(BlueprintPure, Category = "Hacks")
	FHackDefinition GetHackDefinition(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Hacks")
	TArray<int32> GetKeySequence(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Hacks")
	TArray<int32> GetCurrentInput() const { return CurrentInput; }

	/** True if this hack still matches what has been typed and can be activated. */
	UFUNCTION(BlueprintPure, Category = "Hacks")
	bool IsHackViable(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Hacks")
	bool IsHackActive(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Hacks")
	bool IsOnCooldown(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Hacks")
	float GetRemainingCooldown(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Hacks")
	float GetRemainingDuration(int32 Index) const;

#if !UE_BUILD_SHIPPING
	// Debug only 
	void DebugResetCooldowns();
#endif

	UPROPERTY(BlueprintAssignable, Category = "Hacks")
	FOnPanelToggled OnPanelToggled;

	UPROPERTY(BlueprintAssignable, Category = "Hacks")
	FOnPatternsRegenerated OnPatternsRegenerated;

	UPROPERTY(BlueprintAssignable, Category = "Hacks")
	FOnInputChanged OnInputChanged;

	UPROPERTY(BlueprintAssignable, Category = "Hacks")
	FOnHackFailed OnHackFailed;

	UPROPERTY(BlueprintAssignable, Category = "Hacks")
	FOnHackActivated OnHackActivated;

	UPROPERTY(BlueprintAssignable, Category = "Hacks")
	FOnHackExpired OnHackExpired;

	UPROPERTY(BlueprintAssignable, Category = "Hacks")
	FOnCooldownFinished OnCooldownFinished;

protected:
	virtual void BeginPlay() override;

	/** The four hacks, authored in the editor. Order is the panel order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hacks")
	TArray<FHackDefinition> Hacks;

	/** Prints which hack fired to the screen. Compiled out of shipping builds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hacks|Debug")
	bool bShowDebugMessages = true;

	/** Keys per generated sequence. A difficulty dial - longer costs damage uptime. */
	UPROPERTY(EditDefaultsOnly, Category = "Hacks", meta = (ClampMin = "1", ClampMax = "6"))
	int32 SequenceLength = 5;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hacks|Shield",
		meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.MeshComponent"))
	FComponentReference ShieldMeshReference;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hacks|Shield")
	TObjectPtr<UMaterialInterface> ShieldMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hacks|Shield")
	TObjectPtr<UMaterialInterface> InvisibleMaterial;

	UPROPERTY(EditAnywhere, Category = "Hacks|Audio")
	TObjectPtr<USoundBase> TypingSound;

	UPROPERTY(EditAnywhere, Category = "Hacks|Audio")
	TObjectPtr<USoundBase> OpenHackSound;

	UPROPERTY(EditAnywhere, Category = "Hacks|Audio")
	TObjectPtr<USoundBase> CloseHackSound;

	UPROPERTY(EditAnywhere, Category = "Hacks|Audio")
	TObjectPtr<USoundBase> ShieldUpSound;

private:
	void RegeneratePatterns();
	void ActivateHack(int32 Index);
	void FailInput();
	void ClearInput();

	void HandleHackExpired(int32 Index);
	void HandleCooldownFinished(int32 Index);

	void ApplyHackEffect(const FHackDefinition& Hack);
	void RemoveHackEffect(const FHackDefinition& Hack);

	bool CanAcceptInput() const;
	bool IsValidIndex(int32 Index) const { return Hacks.IsValidIndex(Index) && Runtime.IsValidIndex(Index); }

	ABossCharacter* GetBoss();
	UHealthComponent* GetOwnerHealth() const;
	
	void SetShieldVisible(bool bVisible);

	UPROPERTY()
	TArray<FHackRuntimeState> Runtime;

	/** Resolved once in BeginPlay - ShieldMeshReference is a name lookup. */
	UPROPERTY()
	TObjectPtr<UMeshComponent> ShieldMesh;

	UPROPERTY()
	TObjectPtr<ABossCharacter> CachedBoss;

	TArray<int32> CurrentInput;

	bool bPanelOpen = false;
	bool bInputLocked = false;
};
