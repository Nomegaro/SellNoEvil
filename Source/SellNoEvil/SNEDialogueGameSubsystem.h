// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SNEPrototypeContentAsset.h"
#include "SNEDialogueGameSubsystem.generated.h"

class APlayerController;
class UUserWidget;
template<typename T> class TSubclassOf;
struct FActorsInitializedParams;

UENUM(BlueprintType)
enum class ESNEChoiceType : uint8
{
	AdvancePhase,
	Investigate,
	Sell,
	NoSell,
	PrepAction,
	LunchOption,
	CleanStoreNow,
	CleanStoreForTomorrow,
	SkipCleanupTomorrow,
	RestartDay
};

UENUM(BlueprintType)
enum class ESNESkill : uint8
{
	Money      UMETA(DisplayName = "MONEY"),
	Energy     UMETA(DisplayName = "ENERGY"),
	Sanity     UMETA(DisplayName = "SANITY"),
	Morality   UMETA(DisplayName = "MORALITY")
};

USTRUCT(BlueprintType)
struct FSNESkillLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	ESNESkill Skill = ESNESkill::Sanity;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	FText Line;
};

USTRUCT(BlueprintType)
struct FSNESkillCheck
{
	GENERATED_BODY()

	// Which meter is being tested.
	UPROPERTY(BlueprintReadWrite, Category = "SNE|Check")
	ESNESkill Skill = ESNESkill::Money;

	// Target number. 6=trivial, 9=easy, 12=medium, 14=hard, 16=heroic, 20=godlike.
	UPROPERTY(BlueprintReadWrite, Category = "SNE|Check")
	int32 DifficultyClass = 12;

	// Red checks fire once per encounter and cannot be retried after failure.
	UPROPERTY(BlueprintReadWrite, Category = "SNE|Check")
	bool bRedCheck = false;

	// Optional context label (e.g. "Read the customer", "Suppress disgust").
	UPROPERTY(BlueprintReadWrite, Category = "SNE|Check")
	FText ContextLabel;
};

USTRUCT(BlueprintType)
struct FSNESkillCheckResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Check")
	bool bPassed = false;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Check")
	ESNESkill Skill = ESNESkill::Money;

	// 2d6 raw roll (2..12) before modifier.
	UPROPERTY(BlueprintReadOnly, Category = "SNE|Check")
	int32 DiceRoll = 0;

	// Skill modifier applied (skill meter scaled into [-3, +6]).
	UPROPERTY(BlueprintReadOnly, Category = "SNE|Check")
	int32 SkillModifier = 0;

	// DiceRoll + SkillModifier.
	UPROPERTY(BlueprintReadOnly, Category = "SNE|Check")
	int32 Total = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Check")
	int32 DifficultyClass = 0;

	// Total - DC. Negative = failed by margin.
	UPROPERTY(BlueprintReadOnly, Category = "SNE|Check")
	int32 Margin = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Check")
	FText ContextLabel;
};

USTRUCT(BlueprintType)
struct FSNEChoiceData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	ESNEChoiceType ChoiceType = ESNEChoiceType::AdvancePhase;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	FName ActionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	bool bEnabled = true;

	// When true, the choice is locked unless the corresponding skill meter
	// is at least RequiredValue. The widget shows the lock prefix either way.
	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	bool bHasSkillRequirement = false;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	ESNESkill RequiredSkill = ESNESkill::Sanity;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	int32 RequiredValue = 0;
};

USTRUCT(BlueprintType)
struct FSNEPresentationData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	int32 DayNumber = 1;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	ESNEDayPhase Phase = ESNEDayPhase::MorningNews;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	FText HeaderText;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	FText BodyText;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	FText MeterSummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	int32 Money = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	int32 Energy = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	int32 Sanity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	int32 Morality = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	float TipChance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	FText CustomerTitle;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	FText ItemTitle;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	bool bCanInvestigate = false;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	TArray<FText> VisibleClues;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	TArray<FSNESkillLine> SkillCommentary;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	TArray<FSNESkillLine> SkillAttributedClues;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Presentation")
	TArray<FSNEChoiceData> Choices;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSNEPresentationChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSNEThoughtsChanged);

UCLASS(BlueprintType)
class SELLNOEVIL_API USNEThoughtDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Thought")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Thought", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Thought")
	ESNESkill ApplicableSkill = ESNESkill::Sanity;

	// Permanent modifier added to GetSkillModifier(ApplicableSkill) once matured.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Thought")
	int32 MaturedModifier = 1;

	// In-game days the thought spends internalizing before maturing. >=1.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Thought", meta = (ClampMin = "1"))
	int32 DaysToInternalize = 2;

	// Penalty applied each day while internalizing (committing to an idea hurts).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Thought")
	FSNEMeterDelta DailyInternalizingPenalty;
};

USTRUCT(BlueprintType)
struct FSNEActiveThought
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Thought")
	TSoftObjectPtr<USNEThoughtDataAsset> Thought;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Thought")
	int32 DaysRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SNE|Thought")
	bool bMatured = false;
};

UCLASS(BlueprintType)
class SELLNOEVIL_API USNEDialogueGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "SNE|Flow")
	void StartDay();

	UFUNCTION(BlueprintCallable, Category = "SNE|Flow")
	void StartDayIfNeeded();

	UFUNCTION(BlueprintCallable, Category = "SNE|Flow")
	void RestartDay();

	UFUNCTION(BlueprintCallable, Category = "SNE|Flow")
	void AdvancePhase();

	UFUNCTION(BlueprintCallable, Category = "SNE|Flow")
	bool TryInvestigate();

	UFUNCTION(BlueprintCallable, Category = "SNE|Flow")
	bool ResolveSellChoice(bool bSell);

	UFUNCTION(BlueprintCallable, Category = "SNE|Check",
		meta = (ToolTip = "Roll 2d6 + skill modifier vs DifficultyClass. Skill modifier is the corresponding meter scaled into [-3,+6]. The result is also stored as commentary on the next presentation refresh."))
	FSNESkillCheckResult RollSkillCheck(const FSNESkillCheck& Check);

	UFUNCTION(BlueprintPure, Category = "SNE|Check")
	int32 GetSkillModifier(ESNESkill Skill) const;

	UFUNCTION(BlueprintCallable, Category = "SNE|Flow")
	bool ChoosePrepAction(FName ActionId);

	UFUNCTION(BlueprintCallable, Category = "SNE|Flow")
	bool ChooseLunchOption(FName OptionId);

	UFUNCTION(BlueprintCallable, Category = "SNE|Flow")
	void ChooseClosingCleanup(bool bCleanStoreForTomorrow);

	UFUNCTION(BlueprintCallable, Category = "SNE|Flow")
	bool ExecuteChoice(int32 ChoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "SNE|Presentation")
	FSNEPresentationData GetCurrentPresentationData() const;

	UFUNCTION(BlueprintCallable, Category = "SNE|Debug")
	void SetRandomSeedForTesting(int32 Seed);

	UFUNCTION(BlueprintCallable, Category = "SNE|Debug")
	void SetEnergyForTesting(int32 NewEnergy);

	UFUNCTION(BlueprintCallable, Category = "SNE|Debug")
	void SetTipChanceForTesting(float NewTipChance);

	UFUNCTION(BlueprintCallable, Category = "SNE|Debug")
	int32 GetPendingDelayedOutcomeCountForTesting() const;

	UFUNCTION(BlueprintCallable, Category = "SNE|Debug")
	int32 GetActiveScenarioIndexForTesting() const;

	UFUNCTION(BlueprintCallable, Category = "SNE|Debug")
	int32 GetActiveSaleValueForTesting() const;

	UFUNCTION(BlueprintCallable, Category = "SNE|Debug")
	void DebugApplyMorningNewsNow();

	// --- Designer-facing debug helpers (safe to call in PIE) ---

	UFUNCTION(BlueprintCallable, Category = "SNE|Debug",
		meta = (ToolTip = "Set player meters directly. Useful for testing visit gating conditions."))
	void DebugSetMeters(int32 InMoney, int32 InEnergy, int32 InSanity, int32 InMorality);

	UFUNCTION(BlueprintCallable, Category = "SNE|Debug",
		meta = (ToolTip = "Jump directly to a specific customer+visit. Force-enters the current shift phase if needed. Returns false if the customer/visit can't be found."))
	bool DebugForceEncounter(FName CustomerId, FName VisitId);

	UFUNCTION(BlueprintCallable, Category = "SNE|Debug",
		meta = (ToolTip = "Jump to a named phase immediately. Does not run intermediate logic."))
	void DebugJumpToPhase(ESNEDayPhase Phase);

	UFUNCTION(BlueprintCallable, Category = "SNE|Debug",
		meta = (ToolTip = "Wipe all recurring-character memory (VisitCount / LastDecision / CompletedVisitIds)."))
	void DebugClearCustomerHistories();

	UFUNCTION(BlueprintPure, Category = "SNE|Debug",
		meta = (ToolTip = "How many times the player has encountered this customer across the current run."))
	int32 DebugGetCustomerVisitCount(FName CustomerId) const;

	// --- Save / Load ---

	UFUNCTION(BlueprintCallable, Category = "SNE|Save",
		meta = (ToolTip = "Serialize full game state (meters, day, phase, customer histories, delayed outcomes) to a JSON file. SlotName is used as the filename; default 'autosave' writes to Saved/SNESaves/autosave.sav."))
	bool SaveToSlot(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "SNE|Save",
		meta = (ToolTip = "Load game state from a slot. Returns false and leaves current state untouched if the slot is missing or corrupt."))
	bool LoadFromSlot(const FString& SlotName);

	UFUNCTION(BlueprintPure, Category = "SNE|Save",
		meta = (ToolTip = "True if a save file exists at the given slot."))
	bool DoesSaveSlotExist(const FString& SlotName) const;

	UFUNCTION(BlueprintCallable, Category = "SNE|Save",
		meta = (ToolTip = "Delete a save slot. Returns true if the file was deleted."))
	bool DeleteSaveSlot(const FString& SlotName);

	UFUNCTION(BlueprintPure, Category = "SNE|Data")
	const USNEPrototypeContentAsset* GetResolvedContent() const;

	UFUNCTION(BlueprintCallable, Category = "SNE|UI")
	void SetRootWidgetClass(TSubclassOf<UUserWidget> InRootWidgetClass);

	void EnsureUIForPlayerController(APlayerController* PlayerController);

	UPROPERTY(BlueprintAssignable, Category = "SNE|Events")
	FSNEPresentationChanged OnPresentationChanged;

	UPROPERTY(BlueprintAssignable, Category = "SNE|Events")
	FSNEThoughtsChanged OnThoughtsChanged;

	// --- Thought Cabinet ---

	UFUNCTION(BlueprintCallable, Category = "SNE|Thoughts",
		meta = (ToolTip = "Begin internalizing a thought. Fails if all slots are full or the same thought is already active. Slots = 1 + Sanity/3, clamped to [1,5]."))
	bool InternalizeThought(USNEThoughtDataAsset* Thought);

	UFUNCTION(BlueprintCallable, Category = "SNE|Thoughts",
		meta = (ToolTip = "Drop a thought, freeing its slot. Removes both internalizing and matured thoughts."))
	bool ForgetThought(USNEThoughtDataAsset* Thought);

	UFUNCTION(BlueprintPure, Category = "SNE|Thoughts")
	int32 GetThoughtSlots() const;

	UFUNCTION(BlueprintPure, Category = "SNE|Thoughts")
	TArray<FSNEActiveThought> GetActiveThoughts() const;

private:
	struct FSNEDelayedOutcomeEntry
	{
		FText LaterText;
		FSNEMeterDelta LaterDelta;
	};

	struct FSNEActiveEncounter
	{
		int32 ScenarioIndex = INDEX_NONE;
		int32 VisitIndex = INDEX_NONE;
		int32 VisitCountAtSelection = 0;
		int32 SelectedItemPoolIndex = INDEX_NONE;
		ESNECustomerIntent Intent = ESNECustomerIntent::Good;
		bool bInvestigated = false;
		bool bResolved = false;
		TArray<FText> VisibleClues;
		TArray<FSNESkillLine> SkillAttributedClues;
	};

	struct FSNECustomerHistory
	{
		int32 VisitCount = 0;
		int32 LastVisitDay = -1;
		ESNEPreviousDecision LastDecision = ESNEPreviousDecision::NeverMet;
		ESNECustomerIntent LastIntent = ESNECustomerIntent::Good;
		TArray<FName> CompletedVisitIds;
	};

	void EnsureContentLoaded();
	void EnterPhase(ESNEDayPhase NewPhase);
	void StartNextDay();
	void ApplyMeterDelta(const FSNEMeterDelta& Delta);
	void RebuildPresentation();
	void BuildShiftEncounterPresentation();
	void AppendSkillCommentary();
	void StartNextEncounterIfNeeded();
	void ApplySkillGatesToChoices();
	void TickThoughts();
	void ApplyDailyThoughtPenalties();
	void FinalizeCurrentEncounter(bool bSold);
	bool AppendDelayedOutcome(const FSNEOutcomeData& Outcome);
	const FSNEOutcomeData& SelectOutcome(const FSNECustomerVisit& Visit, bool bSell, ESNECustomerIntent Intent) const;
	int32 PickEligibleVisitIndex(const FSNECustomerScenario& Scenario, const FSNECustomerHistory& History) const;
	bool IsVisitEligible(const FSNECustomerVisit& Visit, const FSNECustomerScenario& Scenario, const FSNECustomerHistory& History) const;
	bool IsCustomerEligibleToday(const FSNECustomerScenario& Scenario, const FSNECustomerHistory& History, bool bAlreadyDrawnToday) const;
	int32 GetRequiredEncountersForCurrentShift() const;
	// Returns the visit's currently-selected item for the active encounter, or nullptr if none.
	// Uses FSNEActiveEncounter::SelectedItemPoolIndex when the visit has a pool.
	const USNEItemDataAsset* GetActiveEncounterItem() const;
	int32 GetSaleValue(const USNEItemDataAsset* Item) const;
	static FText ResolveItemDisplayName(const USNEItemDataAsset* Item);
	void BuildDailyCustomerOrder();
	void BroadcastPresentation();
	FString MakeMeterSummary() const;
	void ApplyActionCosts(int32 EnergyCost, int32 MoneyCost);
	void ApplyRandomEventIfNeeded();
	void HandleWorldInitializedActors(const FActorsInitializedParams& Params);
	void HandleWorldPostActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);
	APlayerController* ResolveLocalPlayerController(UWorld* InWorld) const;

	const FSNEPrepAction* FindPrepAction(const TArray<FSNEPrepAction>& ActionPool, FName ActionId) const;
	const FSNELunchOption* FindLunchOption(FName OptionId) const;

	UPROPERTY(Transient)
	TObjectPtr<USNEPrototypeContentAsset> RuntimeContentAsset;

	UPROPERTY(Transient)
	FSNEPresentationData PresentationCache;

	TArray<FSNEDelayedOutcomeEntry> PendingDelayedOutcomes;

	UPROPERTY(Transient)
	TArray<int32> DailyCustomerOrder;

	// Persistent across days for the duration of a play session. Keyed on FSNECustomerScenario::Id.
	TMap<FName, FSNECustomerHistory> CustomerHistories;

	FSNEActiveEncounter ActiveEncounter;

	ESNEDayPhase CurrentPhase = ESNEDayPhase::MorningNews;
	int32 DayNumber = 1;
	int32 Money = 0;
	int32 Energy = 0;
	int32 Sanity = 0;
	int32 Morality = 0;
	float TipChance = 0.05f;
	bool bStoreCleanForTomorrow = true;
	bool bFixedSeed = false;
	int32 MorningResolvedCount = 0;
	int32 EveningResolvedCount = 0;
	int32 CurrentEncounterOrderIndex = 0;
	bool bMorningPrepDone = false;
	bool bLunchDone = false;
	bool bNightPrepDone = false;
	bool bClosingDone = false;
	bool bRandomEventApplied = false;
	bool bHasStartedDay = false;
	FText LastEventText;

	FRandomStream RandomStream;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ActiveRootWidget;

	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> PreferredRootWidgetClass;

	// Most recent skill check result. AppendSkillCommentary() surfaces this once,
	// then clears bHasLastCheckResult so it doesn't echo on every refresh.
	FSNESkillCheckResult LastCheckResult;
	bool bHasLastCheckResult = false;

	// Thought Cabinet — active and matured thoughts.
	UPROPERTY(Transient)
	TArray<FSNEActiveThought> ActiveThoughts;
};
