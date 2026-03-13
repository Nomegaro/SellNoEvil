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
	TArray<FSNEChoiceData> Choices;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSNEPresentationChanged);

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

	UFUNCTION(BlueprintPure, Category = "SNE|Data")
	const USNEPrototypeContentAsset* GetResolvedContent() const;

	UFUNCTION(BlueprintCallable, Category = "SNE|UI")
	void SetRootWidgetClass(TSubclassOf<UUserWidget> InRootWidgetClass);

	void EnsureUIForPlayerController(APlayerController* PlayerController);

	UPROPERTY(BlueprintAssignable, Category = "SNE|Events")
	FSNEPresentationChanged OnPresentationChanged;

private:
	struct FSNEDelayedOutcomeEntry
	{
		FText LaterText;
		FSNEMeterDelta LaterDelta;
	};

	struct FSNEActiveEncounter
	{
		int32 ScenarioIndex = INDEX_NONE;
		ESNECustomerIntent Intent = ESNECustomerIntent::Good;
		bool bInvestigated = false;
		bool bResolved = false;
		TArray<FText> VisibleClues;
	};

	void EnsureContentLoaded();
	void EnterPhase(ESNEDayPhase NewPhase);
	void StartNextDay();
	void ApplyMeterDelta(const FSNEMeterDelta& Delta);
	void RebuildPresentation();
	void BuildShiftEncounterPresentation();
	void StartNextEncounterIfNeeded();
	void FinalizeCurrentEncounter(bool bSold);
	bool AppendDelayedOutcome(const FSNEOutcomeData& Outcome);
	const FSNEOutcomeData& SelectOutcome(const FSNECustomerScenario& Scenario, bool bSell, ESNECustomerIntent Intent) const;
	int32 GetRequiredEncountersForCurrentShift() const;
	int32 GetSaleValue(const FSNECustomerScenario& Scenario) const;
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
};
