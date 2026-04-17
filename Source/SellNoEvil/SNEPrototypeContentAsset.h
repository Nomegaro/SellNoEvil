// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SNEItemDataAsset.h"
#include "SNEPrototypeContentAsset.generated.h"

UENUM(BlueprintType)
enum class ESNEDayPhase : uint8
{
	MorningNews     UMETA(DisplayName = "Morning News"),
	MorningPrep     UMETA(DisplayName = "Morning Prep"),
	MorningShift    UMETA(DisplayName = "Morning Shift"),
	Lunch           UMETA(DisplayName = "Lunch"),
	EveningShift    UMETA(DisplayName = "Evening Shift"),
	NightPrep       UMETA(DisplayName = "Night Prep"),
	Closing         UMETA(DisplayName = "Closing"),
	RandomEvent     UMETA(DisplayName = "Random Event"),
	DayEnd          UMETA(DisplayName = "Day End")
};

UENUM(BlueprintType)
enum class ESNECustomerIntent : uint8
{
	Good,
	Bad
};

UENUM(BlueprintType)
enum class ESNEPreviousDecision : uint8
{
	Any        UMETA(DisplayName = "Any (no filter)"),
	Sold       UMETA(DisplayName = "Sold last time"),
	NotSold    UMETA(DisplayName = "Refused to sell last time"),
	NeverMet   UMETA(DisplayName = "Never met this customer before")
};

UENUM(BlueprintType)
enum class ESNEIntentFilter : uint8
{
	Any        UMETA(DisplayName = "Any (no filter)"),
	Good       UMETA(DisplayName = "Last intent was Good"),
	Bad        UMETA(DisplayName = "Last intent was Bad")
};

UENUM(BlueprintType)
enum class ESNEArcEndBehavior : uint8
{
	LoopLast   UMETA(DisplayName = "Loop last visit forever"),
	Remove     UMETA(DisplayName = "Remove from pool after final visit"),
	Silent     UMETA(DisplayName = "Never re-appear but stay known")
};

USTRUCT(BlueprintType)
struct FSNEMeterDelta
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meters", meta = (
		DisplayName = "Money (MNT)",
		ToolTip = "Change to the player's cash. Negative values deduct money."))
	int32 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meters", meta = (
		ToolTip = "Change to the player's energy. Clamped to [0, Max Energy] at apply time."))
	int32 Energy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meters", meta = (
		ToolTip = "Change to the player's sanity. Unbounded."))
	int32 Sanity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meters", meta = (
		ToolTip = "Change to the player's morality. Unbounded."))
	int32 Morality = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meters", meta = (
		DisplayName = "Tip Chance Delta",
		ToolTip = "Additive adjustment to the tip chance (0.0 - 1.0). e.g. 0.05 = +5%.",
		ClampMin = "-1.0", ClampMax = "1.0", UIMin = "-0.5", UIMax = "0.5"))
	float TipChance = 0.0f;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Outcome"))
struct FSNEOutcomeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immediate", meta = (
		DisplayName = "Immediate Text",
		ToolTip = "Shown right after the player makes the choice.",
		MultiLine = true))
	FText ImmediateText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Immediate", meta = (
		DisplayName = "Immediate Meter Change",
		ToolTip = "Meter changes applied right away. Sanity/Morality values here are actually deferred to the next Morning News."))
	FSNEMeterDelta ImmediateDelta;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delayed", meta = (
		DisplayName = "Morning News Text",
		ToolTip = "Shown the next morning as a delayed consequence. Leave empty to skip the news entry.",
		MultiLine = true))
	FText LaterText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delayed", meta = (
		DisplayName = "Morning News Meter Change",
		ToolTip = "Meter changes applied at the next Morning News."))
	FSNEMeterDelta LaterDelta;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Visit Conditions"))
struct FSNEVisitConditions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visit Count", meta = (
		DisplayName = "Min Visit Count",
		ToolTip = "Required minimum number of previous visits by this customer. 0 = no minimum. e.g. 2 = 'only on 3rd visit or later'.",
		ClampMin = "0"))
	int32 MinVisitCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visit Count", meta = (
		DisplayName = "Max Visit Count",
		ToolTip = "Maximum previous visit count that allows this visit. -1 = no cap. e.g. 0 = 'only on very first meeting'.",
		ClampMin = "-1"))
	int32 MaxVisitCount = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Gating", meta = (
		DisplayName = "Min Day Number",
		ToolTip = "Earliest in-game day this visit is eligible. 0 = any day.",
		ClampMin = "0"))
	int32 MinDayNumber = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day Gating", meta = (
		DisplayName = "Max Day Number",
		ToolTip = "Latest in-game day this visit is eligible. -1 = no cap.",
		ClampMin = "-1"))
	int32 MaxDayNumber = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter Gating", meta = (
		DisplayName = "Min Morality",
		ToolTip = "Minimum player morality required. Leave at default for no filter."))
	int32 MinMorality = -1000000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter Gating", meta = (
		DisplayName = "Max Morality",
		ToolTip = "Maximum player morality allowed. Leave at default for no filter."))
	int32 MaxMorality = 1000000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter Gating", meta = (
		DisplayName = "Min Sanity",
		ToolTip = "Minimum player sanity required. Leave at default for no filter."))
	int32 MinSanity = -1000000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter Gating", meta = (
		DisplayName = "Max Sanity",
		ToolTip = "Maximum player sanity allowed. Leave at default for no filter."))
	int32 MaxSanity = 1000000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "History Gating", meta = (
		DisplayName = "Requires Previous Visits",
		ToolTip = "Visit is eligible only if the customer has previously completed ALL of these VisitIds."))
	TArray<FName> RequiresPreviousVisitIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "History Gating", meta = (
		DisplayName = "Blocked By Previous Visits",
		ToolTip = "Visit is blocked if the customer has completed ANY of these VisitIds."))
	TArray<FName> BlockedByPreviousVisitIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "History Gating", meta = (
		DisplayName = "Required Last Decision",
		ToolTip = "Filter by what the player did on the customer's last visit."))
	ESNEPreviousDecision RequiredLastDecision = ESNEPreviousDecision::Any;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "History Gating", meta = (
		DisplayName = "Required Last Intent",
		ToolTip = "Filter by what the customer's intent was on their last visit."))
	ESNEIntentFilter RequiredLastIntent = ESNEIntentFilter::Any;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Customer Visit"))
struct FSNECustomerVisit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		DisplayName = "Visit Id",
		ToolTip = "Stable identifier for this visit. Referenced by Requires/BlockedBy lists on other visits. e.g. 'first_meeting', 'after_betrayal'."))
	FName VisitId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		ToolTip = "When multiple visits are eligible at the same time, the one with the highest Priority wins."))
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gating", meta = (
		ToolTip = "When this visit is eligible. Leave defaults for 'always eligible once unlocked'."))
	FSNEVisitConditions Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story|Setup", meta = (
		DisplayName = "Requested Item Pool",
		ToolTip = "Items the customer may ask about. 1 entry = deterministic. 2+ entries = a random item is picked each time the visit fires."))
	TArray<TSoftObjectPtr<USNEItemDataAsset>> RequestedItemPool;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story|Setup", meta = (
		DisplayName = "Good Intent Chance",
		ToolTip = "Base probability this visit's customer has 'Good' intent. Modified at runtime by morality/sanity/money/store state.",
		ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.05", UIMax = "0.95"))
	float GoodIntentChance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story|Setup", meta = (
		DisplayName = "Opening Dialogue",
		ToolTip = "First line shown to the player when this visit begins.",
		MultiLine = true))
	FText OpeningDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story|Clues", meta = (
		DisplayName = "Neutral Clues",
		ToolTip = "Investigate can reveal any of these, regardless of intent."))
	TArray<FText> NeutralClues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story|Clues", meta = (
		DisplayName = "Good-Leaning Clues",
		ToolTip = "Extra clues added to the pool when the customer's intent is Good."))
	TArray<FText> GoodLeaningClues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story|Clues", meta = (
		DisplayName = "Bad-Leaning Clues",
		ToolTip = "Extra clues added to the pool when the customer's intent is Bad."))
	TArray<FText> BadLeaningClues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcomes", meta = (
		DisplayName = "Sell -> Good Intent",
		ToolTip = "Outcome when the player chooses to Sell and the customer turned out to be Good."))
	FSNEOutcomeData SellGoodIntentOutcome;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcomes", meta = (
		DisplayName = "Sell -> Bad Intent",
		ToolTip = "Outcome when the player chooses to Sell and the customer turned out to be Bad."))
	FSNEOutcomeData SellBadIntentOutcome;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcomes", meta = (
		DisplayName = "No Sell -> Good Intent",
		ToolTip = "Outcome when the player refuses to sell and the customer was Good."))
	FSNEOutcomeData NoSellGoodIntentOutcome;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcomes", meta = (
		DisplayName = "No Sell -> Bad Intent",
		ToolTip = "Outcome when the player refuses to sell and the customer was Bad."))
	FSNEOutcomeData NoSellBadIntentOutcome;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Customer Scenario"))
struct FSNECustomerScenario
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		DisplayName = "Customer Id",
		ToolTip = "Stable identifier for the customer across their arc. e.g. 'quiet_student'. Used to persist visit history."))
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		ToolTip = "Short display name shown in UI. e.g. 'Architecture Student'."))
	FText Nickname;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		ToolTip = "Age in years. Cosmetic; currently shown in UI only.",
		ClampMin = "0", UIMax = "120"))
	int32 Age = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (
		DisplayName = "Tip Chance Multiplier",
		ToolTip = "Per-customer scale on the current tip chance. 1.0 = neutral, >1 = more generous, <1 = stingy.",
		ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0"))
	float TipChanceMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (
		DisplayName = "Allow Multiple Visits Per Day",
		ToolTip = "If true, this customer may be drawn more than once in a single day. Default: one visit per day at most."))
	bool bAllowMultipleVisitsPerDay = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior", meta = (
		DisplayName = "Arc End Behavior",
		ToolTip = "What happens after the customer has completed all their visits and no later visit is eligible."))
	ESNEArcEndBehavior ArcEndBehavior = ESNEArcEndBehavior::LoopLast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arc", meta = (
		ToolTip = "Ordered arc. Selection walks this list and chooses the highest-priority Visit whose conditions match the current state. If none match, the customer is skipped.",
		TitleProperty = "VisitId"))
	TArray<FSNECustomerVisit> Visits;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Prep Action"))
struct FSNEPrepAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		DisplayName = "Action Id",
		ToolTip = "Stable identifier for this prep action. e.g. 'morning_hot_tea'."))
	FName ActionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		ToolTip = "Short label shown on the choice button."))
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		ToolTip = "Longer description shown after the action resolves.",
		MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Costs", meta = (
		DisplayName = "Energy Cost",
		ClampMin = "0"))
	int32 EnergyCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Costs", meta = (
		DisplayName = "Money Cost (MNT)",
		ClampMin = "0"))
	int32 MoneyCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result", meta = (
		DisplayName = "Meter Result",
		ToolTip = "Meter changes applied when this action resolves (after costs)."))
	FSNEMeterDelta ResultDelta;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Lunch Option"))
struct FSNELunchOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		DisplayName = "Option Id",
		ToolTip = "Stable identifier for this lunch option. e.g. 'lunch_quick_snack'."))
	FName OptionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		ToolTip = "Short label shown on the choice button."))
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		ToolTip = "Longer description shown after the choice resolves.",
		MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Costs", meta = (
		DisplayName = "Energy Cost",
		ClampMin = "0"))
	int32 EnergyCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Costs", meta = (
		DisplayName = "Money Cost (MNT)",
		ClampMin = "0"))
	int32 MoneyCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result", meta = (
		DisplayName = "Meter Result"))
	FSNEMeterDelta ResultDelta;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Random Event"))
struct FSNERandomEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		DisplayName = "Event Id",
		ToolTip = "Stable identifier for this event."))
	FName EventId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		DisplayName = "Event Text",
		ToolTip = "Narrative shown when this event fires.",
		MultiLine = true))
	FText EventText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result", meta = (
		DisplayName = "Meter Result"))
	FSNEMeterDelta ResultDelta;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Prototype Defaults"))
struct FSNEPrototypeDefaults
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Of Run", meta = (
		DisplayName = "Starting Day Number",
		ToolTip = "Day the run begins on. Usually 1, but can be set higher for debug starts.",
		ClampMin = "1"))
	int32 StartingDayNumber = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Of Run", meta = (
		DisplayName = "Starting Money (MNT)",
		ClampMin = "0"))
	int32 StartingMoney = 50000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Of Run", meta = (
		DisplayName = "Starting Energy",
		ClampMin = "0"))
	int32 StartingEnergy = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Of Run", meta = (
		DisplayName = "Starting Sanity"))
	int32 StartingSanity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Of Run", meta = (
		DisplayName = "Starting Morality"))
	int32 StartingMorality = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Of Run", meta = (
		DisplayName = "Start With Clean Store",
		ToolTip = "If true, Day 1 starts with a clean store (no forced morning cleanup)."))
	bool bStartWithCleanStore = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limits", meta = (
		DisplayName = "Max Energy",
		ToolTip = "Energy is clamped between 0 and this value.",
		ClampMin = "1"))
	int32 MaxEnergy = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Costs", meta = (
		DisplayName = "Investigate Energy Cost",
		ClampMin = "0"))
	int32 InvestigateEnergyCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Costs", meta = (
		DisplayName = "Cleanup Energy Cost",
		ClampMin = "0"))
	int32 CleanupEnergyCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shift Size", meta = (
		DisplayName = "Morning Customer Count",
		ToolTip = "Number of customer encounters scheduled for the morning shift.",
		ClampMin = "0"))
	int32 MorningCustomerCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shift Size", meta = (
		DisplayName = "Evening Customer Count",
		ToolTip = "Number of customer encounters scheduled for the evening shift.",
		ClampMin = "0"))
	int32 EveningCustomerCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tips", meta = (
		DisplayName = "Base Tip Chance",
		ToolTip = "Base probability (0.0 - 1.0) that any sale yields a tip, before multipliers.",
		ClampMin = "0.0", ClampMax = "1.0"))
	float BaseTipChance = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intent Tuning", meta = (
		DisplayName = "Morality -> Good Intent Weight",
		ToolTip = "Per-point of player Morality, shift Good Intent chance by this much. Clamped to +/-0.20 total."))
	float MoralityGoodIntentInfluence = 0.002f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intent Tuning", meta = (
		DisplayName = "Sanity -> Good Intent Weight",
		ToolTip = "Per-point of player Sanity, shift Good Intent chance by this much. Clamped to +/-0.15 total."))
	float SanityGoodIntentInfluence = 0.001f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intent Tuning", meta = (
		DisplayName = "Low Money Threshold",
		ToolTip = "Below this cash amount (MNT), the Low Money penalty below kicks in.",
		ClampMin = "0"))
	int32 LowMoneyThresholdForRisk = 30000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intent Tuning", meta = (
		DisplayName = "Low Money Good Intent Penalty",
		ToolTip = "Absolute penalty to Good Intent chance when the player is below the threshold.",
		ClampMin = "0.0", ClampMax = "1.0"))
	float LowMoneyGoodIntentPenalty = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Intent Tuning", meta = (
		DisplayName = "Dirty Store Good Intent Penalty",
		ToolTip = "Absolute penalty to Good Intent chance when the store wasn't cleaned the night before.",
		ClampMin = "0.0", ClampMax = "1.0"))
	float DirtyStoreGoodIntentPenalty = 0.05f;
};

UCLASS(BlueprintType)
class SELLNOEVIL_API USNEPrototypeContentAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defaults", meta = (
		ToolTip = "Tunable defaults for the whole run (starting meters, shift sizes, intent weights, etc.)."))
	FSNEPrototypeDefaults Defaults;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customers", meta = (
		ToolTip = "Customer scenarios. Each has an ordered arc of Visits that unlock based on conditions.",
		TitleProperty = "Id"))
	TArray<FSNECustomerScenario> Customers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Morning Prep", meta = (
		DisplayName = "Morning Prep Actions",
		TitleProperty = "ActionId"))
	TArray<FSNEPrepAction> MorningPrepActions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night Prep", meta = (
		DisplayName = "Night Prep Actions",
		TitleProperty = "ActionId"))
	TArray<FSNEPrepAction> NightPrepActions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lunch", meta = (
		DisplayName = "Lunch Options",
		TitleProperty = "OptionId"))
	TArray<FSNELunchOption> LunchOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (
		DisplayName = "Random Events",
		TitleProperty = "EventId"))
	TArray<FSNERandomEvent> RandomEvents;

	static USNEPrototypeContentAsset* CreateRuntimeDefaultContent(UObject* Outer);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	// Populate this Data Asset with 5 sample customers + create missing DA_Item_* sibling assets in /Game/Items/.
	// Overwrites the Customers array. Editor-only, intended for prototyping.
	UFUNCTION(CallInEditor, Category = "SNE|Authoring", meta = (
		DisplayName = "Generate Sample Customers",
		ToolTip = "Overwrites the Customers array with 5 sample customers and auto-creates any missing item Data Assets in /Game/Items/."))
	void GenerateSampleCustomers();
#endif
};
