// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SNEPrototypeContentAsset.generated.h"

UENUM(BlueprintType)
enum class ESNEDayPhase : uint8
{
	MorningNews,
	MorningPrep,
	MorningShift,
	Lunch,
	EveningShift,
	NightPrep,
	Closing,
	RandomEvent,
	DayEnd
};

UENUM(BlueprintType)
enum class ESNECustomerIntent : uint8
{
	Good,
	Bad
};

UENUM(BlueprintType)
enum class ESNEPriceTier : uint8
{
	Cheap,
	Moderate,
	Expensive
};

USTRUCT(BlueprintType)
struct FSNEMeterDelta
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Meters")
	int32 Money = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Meters")
	int32 Energy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Meters")
	int32 Sanity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Meters")
	int32 Morality = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Meters")
	float TipChance = 0.0f;
};

USTRUCT(BlueprintType)
struct FSNEOutcomeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Outcome")
	FText ImmediateText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Outcome")
	FSNEMeterDelta ImmediateDelta;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Outcome")
	FText LaterText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Outcome")
	FSNEMeterDelta LaterDelta;
};

USTRUCT(BlueprintType)
struct FSNECustomerScenario
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Customer")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Customer")
	FText Nickname;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Customer")
	int32 Age = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Customer")
	FText ItemRequested;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Customer")
	ESNEPriceTier PriceTier = ESNEPriceTier::Cheap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Customer")
	float TipChanceMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Customer")
	FText OpeningDialogue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Customer")
	TArray<FText> NeutralClues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Customer")
	TArray<FText> GoodLeaningClues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Customer")
	TArray<FText> BadLeaningClues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Outcome")
	FSNEOutcomeData SellGoodIntentOutcome;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Outcome")
	FSNEOutcomeData SellBadIntentOutcome;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Outcome")
	FSNEOutcomeData NoSellGoodIntentOutcome;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Outcome")
	FSNEOutcomeData NoSellBadIntentOutcome;
};

USTRUCT(BlueprintType)
struct FSNEPrepAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Prep")
	FName ActionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Prep")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Prep")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Prep")
	int32 EnergyCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Prep")
	int32 MoneyCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Prep")
	FSNEMeterDelta ResultDelta;
};

USTRUCT(BlueprintType)
struct FSNELunchOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Lunch")
	FName OptionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Lunch")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Lunch")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Lunch")
	int32 EnergyCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Lunch")
	int32 MoneyCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Lunch")
	FSNEMeterDelta ResultDelta;
};

USTRUCT(BlueprintType)
struct FSNERandomEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Event")
	FName EventId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Event")
	FText EventText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Event")
	FSNEMeterDelta ResultDelta;
};

USTRUCT(BlueprintType)
struct FSNEPrototypeDefaults
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 StartingMoney = 50000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 StartingEnergy = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 StartingSanity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 StartingMorality = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 MaxEnergy = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	float BaseTipChance = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 InvestigateEnergyCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 CleanupEnergyCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 MorningCustomerCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 EveningCustomerCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	bool bStartWithCleanStore = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 CheapSaleValue = 80000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 ModerateSaleValue = 170000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	int32 ExpensiveSaleValue = 320000;
};

UCLASS(BlueprintType)
class SELLNOEVIL_API USNEPrototypeContentAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Defaults")
	FSNEPrototypeDefaults Defaults;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Customers")
	TArray<FSNECustomerScenario> Customers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Prep")
	TArray<FSNEPrepAction> MorningPrepActions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Prep")
	TArray<FSNEPrepAction> NightPrepActions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Lunch")
	TArray<FSNELunchOption> LunchOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SNE|Events")
	TArray<FSNERandomEvent> RandomEvents;

	static USNEPrototypeContentAsset* CreateRuntimeDefaultContent(UObject* Outer);
	int32 GetSaleValue(ESNEPriceTier PriceTier) const;
};
