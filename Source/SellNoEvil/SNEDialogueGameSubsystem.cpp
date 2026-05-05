// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEDialogueGameSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SNEGameRootWidget.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "SNEDialogueSubsystem"

namespace SNESubsystemInternal
{
	static const TCHAR* DefaultContentAssetPath = TEXT("/Game/Data/DA_SNEPrototypeContent.DA_SNEPrototypeContent");
	static const TCHAR* DefaultContentAssetPathShort = TEXT("/Game/Data/DA_SNEPrototypeContent");
	static const TCHAR* DefaultRootWidgetClassPath = TEXT("/Game/UI/WBP_GameRoot.WBP_GameRoot_C");

	static USNEPrototypeContentAsset* TryLoadContentAssetAtPath(const TCHAR* ObjectPath)
	{
		UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, ObjectPath);
		return Cast<USNEPrototypeContentAsset>(LoadedObject);
	}

	static USNEPrototypeContentAsset* TryLoadAuthoredContentAsset()
	{
		if (USNEPrototypeContentAsset* Content = TryLoadContentAssetAtPath(DefaultContentAssetPath))
		{
			return Content;
		}

		// Support both explicit object path and short package path forms.
		return TryLoadContentAssetAtPath(DefaultContentAssetPathShort);
	}

	static TSubclassOf<UUserWidget> ResolveFallbackRootWidgetClass()
	{
		if (UClass* BlueprintWidgetClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr, DefaultRootWidgetClassPath))
		{
			return BlueprintWidgetClass;
		}

		// Last-resort native fallback only if the native class is concrete.
		UClass* NativeWidgetClass = USNEGameRootWidget::StaticClass();
		if (NativeWidgetClass != nullptr
			&& !NativeWidgetClass->HasAnyClassFlags(CLASS_Abstract)
			&& NativeWidgetClass->IsChildOf(UUserWidget::StaticClass()))
		{
			return NativeWidgetClass;
		}

		return nullptr;
	}

	static bool HasMeaningfulDelta(const FSNEMeterDelta& Delta)
	{
		return Delta.Money != 0
			|| Delta.Energy != 0
			|| Delta.Sanity != 0
			|| Delta.Morality != 0
			|| !FMath::IsNearlyZero(Delta.TipChance);
	}

	static FString BuildDeltaSummary(const FSNEMeterDelta& Delta)
	{
		TArray<FString> Parts;
		const auto AddIntPart = [&Parts](const TCHAR* Label, const int32 Value)
		{
			if (Value != 0)
			{
				Parts.Add(FString::Printf(TEXT("%s %s%d"), Label, Value > 0 ? TEXT("+") : TEXT(""), Value));
			}
		};

		AddIntPart(TEXT("Money"), Delta.Money);
		AddIntPart(TEXT("Energy"), Delta.Energy);
		AddIntPart(TEXT("Sanity"), Delta.Sanity);
		AddIntPart(TEXT("Morality"), Delta.Morality);

		if (!FMath::IsNearlyZero(Delta.TipChance))
		{
			Parts.Add(FString::Printf(TEXT("TipChance %+.0f%%"), Delta.TipChance * 100.0f));
		}

		if (Parts.Num() == 0)
		{
			return TEXT("No meter change");
		}

		return FString::Join(Parts, TEXT(" | "));
	}

	static float ComputeGoodIntentChance(
		const float VisitGoodIntentChance,
		const FSNEPrototypeDefaults& Defaults,
		const int32 InSanity,
		const int32 InMorality,
		const int32 InMoney,
		const bool bInStoreCleanForTomorrow)
	{
		float GoodIntentChance = FMath::Clamp(VisitGoodIntentChance, 0.05f, 0.95f);

		const float MoralityShift = FMath::Clamp(static_cast<float>(InMorality) * Defaults.MoralityGoodIntentInfluence, -0.20f, 0.20f);
		const float SanityShift = FMath::Clamp(static_cast<float>(InSanity) * Defaults.SanityGoodIntentInfluence, -0.15f, 0.15f);
		GoodIntentChance += MoralityShift + SanityShift;

		if (InMoney <= Defaults.LowMoneyThresholdForRisk)
		{
			GoodIntentChance -= FMath::Max(0.0f, Defaults.LowMoneyGoodIntentPenalty);
		}

		if (!bInStoreCleanForTomorrow)
		{
			GoodIntentChance -= FMath::Max(0.0f, Defaults.DirtyStoreGoodIntentPenalty);
		}

		return FMath::Clamp(GoodIntentChance, 0.05f, 0.95f);
	}

	static FText BuildDeferredEthicsNewsText(
		const FSNECustomerScenario& Scenario,
		const FText& ItemDisplayName,
		const bool bSold,
		const ESNECustomerIntent Intent,
		const FSNEMeterDelta& DeferredEthicsDelta)
	{
		FText Headline;
		if (bSold && Intent == ESNECustomerIntent::Bad)
		{
			Headline = LOCTEXT("DeferredNewsSellBad", "News: restricted goods were used in a crime nearby.");
		}
		else if (bSold && Intent == ESNECustomerIntent::Good)
		{
			Headline = LOCTEXT("DeferredNewsSellGood", "News: your sale helped someone avoid serious harm.");
		}
		else if (!bSold && Intent == ESNECustomerIntent::Bad)
		{
			Headline = LOCTEXT("DeferredNewsNoSellBad", "News: someone failed to buy restricted goods from local shops.");
		}
		else
		{
			Headline = LOCTEXT("DeferredNewsNoSellGood", "News: a person in need was turned away, and people are upset.");
		}

		FText Reflection;
		if (DeferredEthicsDelta.Sanity < 0 && DeferredEthicsDelta.Morality < 0)
		{
			Reflection = LOCTEXT("DeferredNewsReflectBothDown", "This stays with you. You feel worse mentally and morally.");
		}
		else if (DeferredEthicsDelta.Sanity > 0 && DeferredEthicsDelta.Morality > 0)
		{
			Reflection = LOCTEXT("DeferredNewsReflectBothUp", "You feel calmer, and your choice feels right.");
		}
		else if (DeferredEthicsDelta.Sanity < 0)
		{
			Reflection = LOCTEXT("DeferredNewsReflectSanityDown", "You can explain your choice, but your sanity is lower.");
		}
		else if (DeferredEthicsDelta.Morality < 0)
		{
			Reflection = LOCTEXT("DeferredNewsReflectMoralityDown", "You made money, but the choice feels wrong.");
		}
		else if (DeferredEthicsDelta.Sanity > 0)
		{
			Reflection = LOCTEXT("DeferredNewsReflectSanityUp", "After seeing what happened, you feel calmer.");
		}
		else
		{
			Reflection = LOCTEXT("DeferredNewsReflectMoralityUp", "After the follow-up, your decision feels more ethical.");
		}

		return FText::Format(
			LOCTEXT("DeferredNewsFmt", "Morning News Follow-up:\n{0}\n\n{1}\n\nCase: {2} asking for {3}."),
			Headline,
			Reflection,
			Scenario.Nickname,
			ItemDisplayName);
	}
}

void USNEDialogueGameSubsystem::StartDay()
{
	EnsureContentLoaded();
	if (RuntimeContentAsset == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: StartDay aborted, RuntimeContentAsset is null"));
		return;
	}

	if (!bFixedSeed)
	{
		RandomStream.Initialize(static_cast<int32>(FPlatformTime::Cycles64() & 0x7fffffff));
	}

	const FSNEPrototypeDefaults& Defaults = RuntimeContentAsset->Defaults;

	DayNumber = FMath::Max(1, Defaults.StartingDayNumber);
	CurrentPhase = ESNEDayPhase::MorningNews;
	Money = Defaults.StartingMoney;
	Energy = Defaults.StartingEnergy;
	Sanity = Defaults.StartingSanity;
	Morality = Defaults.StartingMorality;
	TipChance = Defaults.BaseTipChance;
	bStoreCleanForTomorrow = Defaults.bStartWithCleanStore;

	MorningResolvedCount = 0;
	EveningResolvedCount = 0;
	CurrentEncounterOrderIndex = 0;
	bMorningPrepDone = false;
	bLunchDone = false;
	bNightPrepDone = false;
	bClosingDone = false;
	bRandomEventApplied = false;
	LastEventText = FText::GetEmpty();
	PendingDelayedOutcomes.Reset();
	ActiveEncounter = FSNEActiveEncounter{};
	// StartDay (including RestartDay) wipes recurring-character memory. StartNextDay preserves it.
	CustomerHistories.Reset();

	BuildDailyCustomerOrder();
	bHasStartedDay = true;
	UE_LOG(LogTemp, Log, TEXT("SNE: StartDay success. Customers=%d"), RuntimeContentAsset->Customers.Num());
	EnterPhase(ESNEDayPhase::MorningNews);
}

void USNEDialogueGameSubsystem::StartDayIfNeeded()
{
	if (!bHasStartedDay)
	{
		StartDay();
	}
}

void USNEDialogueGameSubsystem::RestartDay()
{
	StartDay();
}

void USNEDialogueGameSubsystem::AdvancePhase()
{
	switch (CurrentPhase)
	{
	case ESNEDayPhase::MorningNews:
		EnterPhase(ESNEDayPhase::MorningPrep);
		return;

	case ESNEDayPhase::MorningPrep:
		if (bMorningPrepDone)
		{
			EnterPhase(ESNEDayPhase::MorningShift);
		}
		return;

	case ESNEDayPhase::MorningShift:
		if (ActiveEncounter.bResolved)
		{
			StartNextEncounterIfNeeded();
			if (MorningResolvedCount >= GetRequiredEncountersForCurrentShift() && ActiveEncounter.ScenarioIndex == INDEX_NONE)
			{
				EnterPhase(ESNEDayPhase::Lunch);
				return;
			}
			RebuildPresentation();
			BroadcastPresentation();
		}
		return;

	case ESNEDayPhase::Lunch:
		if (bLunchDone)
		{
			EnterPhase(ESNEDayPhase::EveningShift);
		}
		return;

	case ESNEDayPhase::EveningShift:
		if (ActiveEncounter.bResolved)
		{
			StartNextEncounterIfNeeded();
			if (EveningResolvedCount >= GetRequiredEncountersForCurrentShift() && ActiveEncounter.ScenarioIndex == INDEX_NONE)
			{
				EnterPhase(ESNEDayPhase::NightPrep);
				return;
			}
			RebuildPresentation();
			BroadcastPresentation();
		}
		return;

	case ESNEDayPhase::NightPrep:
		if (bNightPrepDone)
		{
			EnterPhase(ESNEDayPhase::Closing);
		}
		return;

	case ESNEDayPhase::Closing:
		if (bClosingDone)
		{
			EnterPhase(ESNEDayPhase::RandomEvent);
		}
		return;

	case ESNEDayPhase::RandomEvent:
		EnterPhase(ESNEDayPhase::DayEnd);
		return;

	case ESNEDayPhase::DayEnd:
		StartNextDay();
		return;

	default:
		return;
	}
}

bool USNEDialogueGameSubsystem::TryInvestigate()
{
	EnsureContentLoaded();
	if (RuntimeContentAsset == nullptr || (CurrentPhase != ESNEDayPhase::MorningShift && CurrentPhase != ESNEDayPhase::EveningShift))
	{
		return false;
	}

	const int32 ScenarioIndex = ActiveEncounter.ScenarioIndex;
	const bool bCanInvestigate = ScenarioIndex != INDEX_NONE && !ActiveEncounter.bResolved && !ActiveEncounter.bInvestigated;
	if (!bCanInvestigate || Energy < RuntimeContentAsset->Defaults.InvestigateEnergyCost)
	{
		return false;
	}

	if (!RuntimeContentAsset->Customers.IsValidIndex(ScenarioIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: TryInvestigate aborted due to invalid scenario index %d."), ScenarioIndex);
		LastEventText = LOCTEXT("InvestigateInvalidEncounter", "Could not investigate because encounter data is missing.");
		ActiveEncounter = FSNEActiveEncounter{};
		ActiveEncounter.bResolved = true;
		RebuildPresentation();
		BroadcastPresentation();
		return false;
	}

	const FSNECustomerScenario& Scenario = RuntimeContentAsset->Customers[ScenarioIndex];
	if (!Scenario.Visits.IsValidIndex(ActiveEncounter.VisitIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: TryInvestigate aborted due to invalid visit index %d on scenario %s."), ActiveEncounter.VisitIndex, *Scenario.Id.ToString());
		LastEventText = LOCTEXT("InvestigateInvalidVisit", "Could not investigate because visit data is missing.");
		return false;
	}
	const FSNECustomerVisit& Visit = Scenario.Visits[ActiveEncounter.VisitIndex];

	Energy = FMath::Clamp(Energy - RuntimeContentAsset->Defaults.InvestigateEnergyCost, 0, RuntimeContentAsset->Defaults.MaxEnergy);

	TArray<FText> Pool = Visit.NeutralClues;
	if (ActiveEncounter.Intent == ESNECustomerIntent::Good)
	{
		Pool.Append(Visit.GoodLeaningClues);
	}
	else
	{
		Pool.Append(Visit.BadLeaningClues);
	}

	ActiveEncounter.VisibleClues.Reset();
	ActiveEncounter.SkillAttributedClues.Reset();
	if (Pool.Num() == 0)
	{
		ActiveEncounter.bInvestigated = true;
		LastEventText = LOCTEXT("InvestigateNoClues", "You investigate, but this customer gives you nothing useful.");
		RebuildPresentation();
		BroadcastPresentation();
		return true;
	}

	// Skill attribution: which voice "finds" each clue.
	// Neutral observations -> Sanity (something feels off).
	// Good-leaning sympathy -> Morality (you owe them due diligence).
	// Bad-leaning red flags  -> Money (your merchant instincts catch the angle).
	const int32 NumNeutral = Visit.NeutralClues.Num();
	const int32 NumGood    = Visit.GoodLeaningClues.Num();
	auto SkillForPoolIndex = [&](int32 PoolIdx) -> ESNESkill
	{
		if (PoolIdx < NumNeutral) return ESNESkill::Sanity;
		if (ActiveEncounter.Intent == ESNECustomerIntent::Good)
		{
			return ESNESkill::Morality;
		}
		return ESNESkill::Money;
	};

	const int32 DesiredCount = FMath::Clamp(RandomStream.RandRange(2, 3), 1, Pool.Num());
	TArray<int32> Indices;
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		Indices.Add(Index);
	}

	for (int32 Pick = 0; Pick < DesiredCount && Indices.Num() > 0; ++Pick)
	{
		const int32 Roll = RandomStream.RandRange(0, Indices.Num() - 1);
		const int32 PoolIndex = Indices[Roll];
		Indices.RemoveAtSwap(Roll, 1, EAllowShrinking::No);
		ActiveEncounter.VisibleClues.Add(Pool[PoolIndex]);

		FSNESkillLine Attributed;
		Attributed.Skill = SkillForPoolIndex(PoolIndex);
		Attributed.Line = Pool[PoolIndex];
		ActiveEncounter.SkillAttributedClues.Add(Attributed);
	}

	ActiveEncounter.bInvestigated = true;
	LastEventText = LOCTEXT("InvestigateDone", "You study their tells and piece together what you can.");
	RebuildPresentation();
	BroadcastPresentation();
	return true;
}

bool USNEDialogueGameSubsystem::ResolveSellChoice(const bool bSell)
{
	EnsureContentLoaded();
	if (RuntimeContentAsset == nullptr || (CurrentPhase != ESNEDayPhase::MorningShift && CurrentPhase != ESNEDayPhase::EveningShift))
	{
		return false;
	}

	if (ActiveEncounter.ScenarioIndex == INDEX_NONE || ActiveEncounter.bResolved)
	{
		return false;
	}

	FinalizeCurrentEncounter(bSell);
	RebuildPresentation();
	BroadcastPresentation();
	return true;
}

bool USNEDialogueGameSubsystem::ChoosePrepAction(FName ActionId)
{
	EnsureContentLoaded();
	if (RuntimeContentAsset == nullptr || (CurrentPhase != ESNEDayPhase::MorningPrep && CurrentPhase != ESNEDayPhase::NightPrep))
	{
		return false;
	}

	if (CurrentPhase == ESNEDayPhase::MorningPrep && !bStoreCleanForTomorrow)
	{
		if (ActionId != TEXT("forced_cleanup"))
		{
			return false;
		}

		FSNEMeterDelta CleanupCost;
		CleanupCost.Energy = -RuntimeContentAsset->Defaults.CleanupEnergyCost;
		ApplyMeterDelta(CleanupCost);
		bStoreCleanForTomorrow = true;
		bMorningPrepDone = true;
		LastEventText = LOCTEXT("ForcedMorningCleanupText", "You spend your prep slot cleaning the store. It costs effort, but tomorrow starts clean.");
		RebuildPresentation();
		BroadcastPresentation();
		return true;
	}

	if (ActionId == TEXT("skip"))
	{
		if (CurrentPhase == ESNEDayPhase::MorningPrep)
		{
			bMorningPrepDone = true;
		}
		else
		{
			bNightPrepDone = true;
		}
		LastEventText = LOCTEXT("PrepSkipText", "You keep it simple and conserve resources.");
		RebuildPresentation();
		BroadcastPresentation();
		return true;
	}

	const TArray<FSNEPrepAction>& SourceActions = CurrentPhase == ESNEDayPhase::MorningPrep ? RuntimeContentAsset->MorningPrepActions : RuntimeContentAsset->NightPrepActions;
	const FSNEPrepAction* SelectedAction = FindPrepAction(SourceActions, ActionId);
	if (SelectedAction == nullptr)
	{
		return false;
	}

	if (Energy < SelectedAction->EnergyCost || Money < SelectedAction->MoneyCost)
	{
		return false;
	}

	ApplyActionCosts(SelectedAction->EnergyCost, SelectedAction->MoneyCost);
	ApplyMeterDelta(SelectedAction->ResultDelta);
	LastEventText = FText::Format(
		LOCTEXT("PrepResultFmt", "{0}\n\n{1}"),
		SelectedAction->Label,
		SelectedAction->Description);

	if (CurrentPhase == ESNEDayPhase::MorningPrep)
	{
		bMorningPrepDone = true;
	}
	else
	{
		bNightPrepDone = true;
	}

	RebuildPresentation();
	BroadcastPresentation();
	return true;
}

bool USNEDialogueGameSubsystem::ChooseLunchOption(FName OptionId)
{
	EnsureContentLoaded();
	if (RuntimeContentAsset == nullptr || CurrentPhase != ESNEDayPhase::Lunch || bLunchDone)
	{
		return false;
	}

	const FSNELunchOption* Option = FindLunchOption(OptionId);
	if (Option == nullptr || Energy < Option->EnergyCost || Money < Option->MoneyCost)
	{
		return false;
	}

	ApplyActionCosts(Option->EnergyCost, Option->MoneyCost);
	ApplyMeterDelta(Option->ResultDelta);
	bLunchDone = true;
	LastEventText = FText::Format(LOCTEXT("LunchResultFmt", "{0}\n\n{1}"), Option->Label, Option->Description);
	RebuildPresentation();
	BroadcastPresentation();
	return true;
}

void USNEDialogueGameSubsystem::ChooseClosingCleanup(const bool bCleanStoreForTomorrow)
{
	EnsureContentLoaded();
	if (RuntimeContentAsset == nullptr || CurrentPhase != ESNEDayPhase::Closing || bClosingDone)
	{
		return;
	}

	if (bCleanStoreForTomorrow)
	{
		FSNEMeterDelta CleanupCost;
		CleanupCost.Energy = -RuntimeContentAsset->Defaults.CleanupEnergyCost;
		ApplyMeterDelta(CleanupCost);
		bStoreCleanForTomorrow = true;
		LastEventText = LOCTEXT("ClosingCleanText", "You clean the store before leaving. Tomorrow's prep window stays open.");
	}
	else
	{
		bStoreCleanForTomorrow = false;
		LastEventText = LOCTEXT("ClosingSkipText", "You lock up and leave the mess for morning.");
	}

	bClosingDone = true;
	RebuildPresentation();
	BroadcastPresentation();
}

bool USNEDialogueGameSubsystem::ExecuteChoice(const int32 ChoiceIndex)
{
	if (!PresentationCache.Choices.IsValidIndex(ChoiceIndex))
	{
		return false;
	}

	const FSNEChoiceData Choice = PresentationCache.Choices[ChoiceIndex];
	if (!Choice.bEnabled)
	{
		return false;
	}

	switch (Choice.ChoiceType)
	{
	case ESNEChoiceType::AdvancePhase:
		AdvancePhase();
		return true;
	case ESNEChoiceType::Investigate:
		return TryInvestigate();
	case ESNEChoiceType::Sell:
		return ResolveSellChoice(true);
	case ESNEChoiceType::NoSell:
		return ResolveSellChoice(false);
	case ESNEChoiceType::PrepAction:
		return ChoosePrepAction(Choice.ActionId);
	case ESNEChoiceType::LunchOption:
		return ChooseLunchOption(Choice.ActionId);
	case ESNEChoiceType::CleanStoreNow:
		return ChoosePrepAction(TEXT("forced_cleanup"));
	case ESNEChoiceType::CleanStoreForTomorrow:
		ChooseClosingCleanup(true);
		return true;
	case ESNEChoiceType::SkipCleanupTomorrow:
		ChooseClosingCleanup(false);
		return true;
	case ESNEChoiceType::RestartDay:
		RestartDay();
		return true;
	default:
		break;
	}

	return false;
}

FSNEPresentationData USNEDialogueGameSubsystem::GetCurrentPresentationData() const
{
	return PresentationCache;
}

void USNEDialogueGameSubsystem::SetRandomSeedForTesting(const int32 Seed)
{
	bFixedSeed = true;
	RandomStream.Initialize(Seed);
}

void USNEDialogueGameSubsystem::SetEnergyForTesting(const int32 NewEnergy)
{
	EnsureContentLoaded();
	if (RuntimeContentAsset == nullptr)
	{
		return;
	}

	Energy = FMath::Clamp(NewEnergy, 0, RuntimeContentAsset->Defaults.MaxEnergy);
	RebuildPresentation();
	BroadcastPresentation();
}

void USNEDialogueGameSubsystem::SetTipChanceForTesting(const float NewTipChance)
{
	TipChance = FMath::Clamp(NewTipChance, 0.0f, 1.0f);
	RebuildPresentation();
	BroadcastPresentation();
}

int32 USNEDialogueGameSubsystem::GetPendingDelayedOutcomeCountForTesting() const
{
	return PendingDelayedOutcomes.Num();
}

int32 USNEDialogueGameSubsystem::GetActiveScenarioIndexForTesting() const
{
	return ActiveEncounter.ScenarioIndex;
}

int32 USNEDialogueGameSubsystem::GetActiveSaleValueForTesting() const
{
	return GetSaleValue(GetActiveEncounterItem());
}

void USNEDialogueGameSubsystem::DebugApplyMorningNewsNow()
{
	EnterPhase(ESNEDayPhase::MorningNews);
}

const USNEPrototypeContentAsset* USNEDialogueGameSubsystem::GetResolvedContent() const
{
	return RuntimeContentAsset;
}

void USNEDialogueGameSubsystem::SetRootWidgetClass(TSubclassOf<UUserWidget> InRootWidgetClass)
{
	if (InRootWidgetClass != nullptr)
	{
		PreferredRootWidgetClass = InRootWidgetClass;
	}
}

void USNEDialogueGameSubsystem::EnsureUIForPlayerController(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: EnsureUIForPlayerController skipped, invalid PlayerController"));
		return;
	}

	if (!IsValid(ActiveRootWidget))
	{
		TSubclassOf<UUserWidget> WidgetClassToUse = PreferredRootWidgetClass;
		const UClass* RawWidgetClass = WidgetClassToUse.Get();
		const bool bWidgetClassInvalid = RawWidgetClass == nullptr
			|| RawWidgetClass->HasAnyClassFlags(CLASS_Abstract)
			|| !RawWidgetClass->IsChildOf(UUserWidget::StaticClass())
			|| !CreateWidgetHelpers::ValidateUserWidgetClass(RawWidgetClass);
		if (bWidgetClassInvalid)
		{
			const TSubclassOf<UUserWidget> FallbackWidgetClass = SNESubsystemInternal::ResolveFallbackRootWidgetClass();
			if (FallbackWidgetClass == nullptr)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("SNE: PreferredRootWidgetClass is invalid (%s), and fallback widget class at '%s' could not be loaded."),
					*GetNameSafe(RawWidgetClass),
					SNESubsystemInternal::DefaultRootWidgetClassPath);
				return;
			}

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("SNE: PreferredRootWidgetClass is invalid (%s). Falling back to %s."),
				*GetNameSafe(RawWidgetClass),
				*GetNameSafe(FallbackWidgetClass.Get()));
			WidgetClassToUse = FallbackWidgetClass;
			RawWidgetClass = WidgetClassToUse.Get();
		}
		if (RawWidgetClass == nullptr || !CreateWidgetHelpers::ValidateUserWidgetClass(RawWidgetClass))
		{
			UE_LOG(LogTemp, Error, TEXT("SNE: Could not resolve a valid root widget class. UI creation skipped."));
			return;
		}

		ActiveRootWidget = UUserWidget::CreateWidgetInstance(*PlayerController, WidgetClassToUse, TEXT("SNERootWidget"));
		if (IsValid(ActiveRootWidget))
		{
			ActiveRootWidget->SetVisibility(ESlateVisibility::Visible);
			// Force global viewport layer to avoid PIE player-layer routing edge cases.
			ActiveRootWidget->AddToViewport(10000);
			ActiveRootWidget->ForceLayoutPrepass();
			UE_LOG(LogTemp, Log, TEXT("SNE: Created and added root widget: %s (Class=%s)"), *GetNameSafe(ActiveRootWidget), *GetNameSafe(WidgetClassToUse));
			UE_LOG(LogTemp, Log, TEXT("SNE: Root widget in viewport=%d visibility=%d"), ActiveRootWidget->IsInViewport() ? 1 : 0, static_cast<int32>(ActiveRootWidget->GetVisibility()));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SNE: Failed to create root widget"));
		}
	}
	else
	{
		if (!ActiveRootWidget->IsInViewport())
		{
			ActiveRootWidget->AddToViewport(10000);
			UE_LOG(LogTemp, Warning, TEXT("SNE: Root widget existed but was not in viewport, re-added"));
		}
		UE_LOG(LogTemp, Verbose, TEXT("SNE: Reusing existing root widget"));
	}

	PlayerController->SetShowMouseCursor(true);
	PlayerController->SetInputMode(FInputModeGameAndUI());
	UE_LOG(LogTemp, Verbose, TEXT("SNE: Applied GameAndUI input mode"));
}

void USNEDialogueGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("SNE: Dialogue subsystem Initialize"));
	FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &USNEDialogueGameSubsystem::HandleWorldInitializedActors);
	FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &USNEDialogueGameSubsystem::HandleWorldPostActorTick);
}

void USNEDialogueGameSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("SNE: Dialogue subsystem Deinitialize"));
	FWorldDelegates::OnWorldInitializedActors.RemoveAll(this);
	FWorldDelegates::OnWorldPostActorTick.RemoveAll(this);
	ActiveRootWidget = nullptr;
	Super::Deinitialize();
}

void USNEDialogueGameSubsystem::EnsureContentLoaded()
{
	if (RuntimeContentAsset != nullptr)
	{
		return;
	}

	RuntimeContentAsset = SNESubsystemInternal::TryLoadAuthoredContentAsset();
	if (RuntimeContentAsset == nullptr)
	{
		UObject* WrongTypeObject = StaticLoadObject(UObject::StaticClass(), nullptr, SNESubsystemInternal::DefaultContentAssetPath);
		if (WrongTypeObject != nullptr)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("SNE: Object at '%s' is '%s', expected '%s'. Falling back to runtime default content."),
				SNESubsystemInternal::DefaultContentAssetPath,
				*GetNameSafe(WrongTypeObject->GetClass()),
				*USNEPrototypeContentAsset::StaticClass()->GetName());
		}
	}

	if (RuntimeContentAsset == nullptr)
	{
		RuntimeContentAsset = USNEPrototypeContentAsset::CreateRuntimeDefaultContent(this);
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("SNE: Using runtime default content because authored content asset could not be loaded from '%s'."),
			SNESubsystemInternal::DefaultContentAssetPath);
	}
}

void USNEDialogueGameSubsystem::HandleWorldInitializedActors(const FActorsInitializedParams& Params)
{
	UWorld* InWorld = Params.World;
	if (InWorld == nullptr || !InWorld->IsGameWorld())
	{
		return;
	}

	if (InWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	APlayerController* PlayerController = ResolveLocalPlayerController(InWorld);
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: OnWorldInitializedActors fired but no PlayerController yet"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("SNE: OnWorldInitializedActors for world %s"), *InWorld->GetName());
	EnsureUIForPlayerController(PlayerController);
	StartDayIfNeeded();
}

void USNEDialogueGameSubsystem::HandleWorldPostActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds)
{
	if (IsValid(ActiveRootWidget))
	{
		return;
	}

	if (InWorld == nullptr || !InWorld->IsGameWorld())
	{
		return;
	}

	if (InWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	APlayerController* PlayerController = ResolveLocalPlayerController(InWorld);
	if (!IsValid(PlayerController))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("SNE: PostActorTick fallback spawning UI in world %s"), *InWorld->GetName());
	EnsureUIForPlayerController(PlayerController);
	StartDayIfNeeded();
}

APlayerController* USNEDialogueGameSubsystem::ResolveLocalPlayerController(UWorld* InWorld) const
{
	if (InWorld == nullptr)
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = InWorld->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (IsValid(PC) && PC->IsLocalController())
		{
			return PC;
		}
	}

	return UGameplayStatics::GetPlayerController(InWorld, 0);
}

void USNEDialogueGameSubsystem::EnterPhase(const ESNEDayPhase NewPhase)
{
	CurrentPhase = NewPhase;

	switch (CurrentPhase)
	{
	case ESNEDayPhase::MorningNews:
	{
		bMorningPrepDone = false;
		bLunchDone = false;
		bNightPrepDone = false;
		bClosingDone = false;
		bRandomEventApplied = false;
		if (PendingDelayedOutcomes.Num() == 0)
		{
			LastEventText = LOCTEXT("MorningNewsNoUpdates", "Morning News: Quiet headlines today. No delayed consequences arrived.");
		}
		else
		{
			FString Combined;
			for (const FSNEDelayedOutcomeEntry& Entry : PendingDelayedOutcomes)
			{
				ApplyMeterDelta(Entry.LaterDelta);
				if (!Combined.IsEmpty())
				{
					Combined += TEXT("\n\n");
				}
				const FString DeltaSummary = SNESubsystemInternal::BuildDeltaSummary(Entry.LaterDelta);
				const FString EntryText = Entry.LaterText.IsEmpty()
					? LOCTEXT("MorningNewsUnnamedUpdate", "Unreported overnight consequence.").ToString()
					: Entry.LaterText.ToString();
				Combined += FString::Printf(TEXT("%s\nEffect: %s"), *EntryText, *DeltaSummary);
			}
			PendingDelayedOutcomes.Reset();
			LastEventText = FText::Format(LOCTEXT("MorningNewsUpdatesFmt", "Morning News:\n\n{0}"), FText::FromString(Combined));
		}
		break;
	}

	case ESNEDayPhase::MorningPrep:
		if (!bStoreCleanForTomorrow)
		{
			LastEventText = LOCTEXT("MorningPrepForcedMessage", "The store was not cleaned last night. You must clean now before opening.");
		}
		else
		{
			LastEventText = LOCTEXT("MorningPrepStartMessage", "Morning Prep: choose one action before the first rush.");
		}
		break;

	case ESNEDayPhase::MorningShift:
		StartNextEncounterIfNeeded();
		break;

	case ESNEDayPhase::Lunch:
		LastEventText = LOCTEXT("LunchStartMessage", "Lunch Break: take a beat and recover before evening customers.");
		break;

	case ESNEDayPhase::EveningShift:
		StartNextEncounterIfNeeded();
		break;

	case ESNEDayPhase::NightPrep:
		LastEventText = LOCTEXT("NightPrepStartMessage", "Night Prep: one final action before closing decisions.");
		break;

	case ESNEDayPhase::Closing:
		LastEventText = LOCTEXT("ClosingStartMessage", "Closing Time: clean now for tomorrow, or leave it for morning.");
		break;

	case ESNEDayPhase::RandomEvent:
		ApplyRandomEventIfNeeded();
		break;

	case ESNEDayPhase::DayEnd:
		LastEventText = FText::Format(
			LOCTEXT("DayEndSummaryFmt", "Day complete.\nMoney: {0} MNT\nEnergy: {1}\nSanity: {2}\nMorality: {3}\nTip chance for day ended at {4}%"),
			FText::AsNumber(Money),
			FText::AsNumber(Energy),
			FText::AsNumber(Sanity),
			FText::AsNumber(Morality),
			FText::AsNumber(FMath::RoundToInt(TipChance * 100.0f)));
		break;
	default:
		break;
	}

	RebuildPresentation();
	BroadcastPresentation();
}

void USNEDialogueGameSubsystem::StartNextDay()
{
	EnsureContentLoaded();
	if (RuntimeContentAsset == nullptr)
	{
		return;
	}

	++DayNumber;
	const FSNEPrototypeDefaults& Defaults = RuntimeContentAsset->Defaults;

	// Carry long-term meters, but reset daily readiness values.
	Energy = FMath::Clamp(Defaults.StartingEnergy, 0, Defaults.MaxEnergy);
	TipChance = Defaults.BaseTipChance;

	// Thought Cabinet: tick maturation, then apply daily internalizing penalties.
	TickThoughts();
	ApplyDailyThoughtPenalties();

	MorningResolvedCount = 0;
	EveningResolvedCount = 0;
	CurrentEncounterOrderIndex = 0;
	bMorningPrepDone = false;
	bLunchDone = false;
	bNightPrepDone = false;
	bClosingDone = false;
	bRandomEventApplied = false;
	LastEventText = FText::GetEmpty();
	ActiveEncounter = FSNEActiveEncounter{};

	BuildDailyCustomerOrder();
	UE_LOG(LogTemp, Log, TEXT("SNE: Starting next day. Day=%d PendingDelayedOutcomes=%d"), DayNumber, PendingDelayedOutcomes.Num());
	EnterPhase(ESNEDayPhase::MorningNews);
}

void USNEDialogueGameSubsystem::ApplyMeterDelta(const FSNEMeterDelta& Delta)
{
	if (RuntimeContentAsset == nullptr)
	{
		return;
	}

	const int32 MaxEnergy = RuntimeContentAsset->Defaults.MaxEnergy;
	Money += Delta.Money;
	Energy = FMath::Clamp(Energy + Delta.Energy, 0, MaxEnergy);
	Sanity += Delta.Sanity;
	Morality += Delta.Morality;
	TipChance = FMath::Clamp(TipChance + Delta.TipChance, 0.0f, 0.95f);
}

void USNEDialogueGameSubsystem::RebuildPresentation()
{
	PresentationCache = FSNEPresentationData{};
	PresentationCache.DayNumber = DayNumber;
	PresentationCache.Phase = CurrentPhase;
	PresentationCache.Money = Money;
	PresentationCache.Energy = Energy;
	PresentationCache.Sanity = Sanity;
	PresentationCache.Morality = Morality;
	PresentationCache.TipChance = TipChance;
	PresentationCache.MeterSummaryText = FText::FromString(MakeMeterSummary());

	FSNEChoiceData Choice;
	switch (CurrentPhase)
	{
	case ESNEDayPhase::MorningNews:
		PresentationCache.HeaderText = LOCTEXT("HeaderMorningNews", "Morning News");
		PresentationCache.BodyText = LastEventText;
		Choice.Label = LOCTEXT("ChoiceContinue", "Continue");
		Choice.ChoiceType = ESNEChoiceType::AdvancePhase;
		PresentationCache.Choices = {Choice};
		break;

	case ESNEDayPhase::MorningPrep:
		PresentationCache.HeaderText = LOCTEXT("HeaderMorningPrep", "Morning Prep");
		PresentationCache.BodyText = LastEventText;
		if (bMorningPrepDone)
		{
			Choice.Label = LOCTEXT("ChoiceOpenStore", "Open Store");
			Choice.ChoiceType = ESNEChoiceType::AdvancePhase;
			PresentationCache.Choices = {Choice};
			break;
		}

		if (!bStoreCleanForTomorrow)
		{
			Choice.Label = FText::Format(
				LOCTEXT("ChoiceForcedCleanupFmt", "Clean Store (-{0} Energy)"),
				FText::AsNumber(RuntimeContentAsset != nullptr ? RuntimeContentAsset->Defaults.CleanupEnergyCost : 1));
			Choice.ChoiceType = ESNEChoiceType::CleanStoreNow;
			Choice.bEnabled = Energy >= (RuntimeContentAsset != nullptr ? RuntimeContentAsset->Defaults.CleanupEnergyCost : 1);
			PresentationCache.Choices = {Choice};
			break;
		}

		for (const FSNEPrepAction& Action : RuntimeContentAsset->MorningPrepActions)
		{
			FSNEChoiceData ActionChoice;
			ActionChoice.ChoiceType = ESNEChoiceType::PrepAction;
			ActionChoice.ActionId = Action.ActionId;
			ActionChoice.bEnabled = Energy >= Action.EnergyCost && Money >= Action.MoneyCost;
			ActionChoice.Label = FText::Format(
				LOCTEXT("MorningActionFmt", "{0} ({1}E, {2} MNT)"),
				Action.Label,
				FText::AsNumber(Action.EnergyCost),
				FText::AsNumber(Action.MoneyCost));
			PresentationCache.Choices.Add(ActionChoice);
		}

		Choice = FSNEChoiceData{};
		Choice.Label = LOCTEXT("ChoiceSkipMorningPrep", "Skip Prep");
		Choice.ChoiceType = ESNEChoiceType::PrepAction;
		Choice.ActionId = TEXT("skip");
		PresentationCache.Choices.Add(Choice);
		break;

	case ESNEDayPhase::MorningShift:
	case ESNEDayPhase::EveningShift:
		BuildShiftEncounterPresentation();
		break;

	case ESNEDayPhase::Lunch:
		PresentationCache.HeaderText = LOCTEXT("HeaderLunch", "Lunch Break");
		PresentationCache.BodyText = LastEventText;
		if (bLunchDone)
		{
			Choice.Label = LOCTEXT("ChoiceBackToWork", "Back To Work");
			Choice.ChoiceType = ESNEChoiceType::AdvancePhase;
			PresentationCache.Choices = {Choice};
			break;
		}
		for (const FSNELunchOption& Option : RuntimeContentAsset->LunchOptions)
		{
			FSNEChoiceData LunchChoice;
			LunchChoice.ChoiceType = ESNEChoiceType::LunchOption;
			LunchChoice.ActionId = Option.OptionId;
			LunchChoice.bEnabled = Energy >= Option.EnergyCost && Money >= Option.MoneyCost;
			LunchChoice.Label = FText::Format(
				LOCTEXT("LunchOptionFmt", "{0} ({1}E, {2} MNT)"),
				Option.Label,
				FText::AsNumber(Option.EnergyCost),
				FText::AsNumber(Option.MoneyCost));
			PresentationCache.Choices.Add(LunchChoice);
		}
		break;

	case ESNEDayPhase::NightPrep:
		PresentationCache.HeaderText = LOCTEXT("HeaderNightPrep", "Night Prep");
		PresentationCache.BodyText = LastEventText;
		if (bNightPrepDone)
		{
			Choice.Label = LOCTEXT("ChoiceToClosing", "Continue To Closing");
			Choice.ChoiceType = ESNEChoiceType::AdvancePhase;
			PresentationCache.Choices = {Choice};
			break;
		}
		for (const FSNEPrepAction& Action : RuntimeContentAsset->NightPrepActions)
		{
			FSNEChoiceData ActionChoice;
			ActionChoice.ChoiceType = ESNEChoiceType::PrepAction;
			ActionChoice.ActionId = Action.ActionId;
			ActionChoice.bEnabled = Energy >= Action.EnergyCost && Money >= Action.MoneyCost;
			ActionChoice.Label = FText::Format(
				LOCTEXT("NightActionFmt", "{0} ({1}E, {2} MNT)"),
				Action.Label,
				FText::AsNumber(Action.EnergyCost),
				FText::AsNumber(Action.MoneyCost));
			PresentationCache.Choices.Add(ActionChoice);
		}

		Choice = FSNEChoiceData{};
		Choice.Label = LOCTEXT("ChoiceSkipNightPrep", "Skip Prep");
		Choice.ChoiceType = ESNEChoiceType::PrepAction;
		Choice.ActionId = TEXT("skip");
		PresentationCache.Choices.Add(Choice);
		break;

	case ESNEDayPhase::Closing:
		PresentationCache.HeaderText = LOCTEXT("HeaderClosing", "Closing Time");
		PresentationCache.BodyText = LastEventText;
		if (bClosingDone)
		{
			Choice.Label = LOCTEXT("ChoiceResolveClosing", "Continue");
			Choice.ChoiceType = ESNEChoiceType::AdvancePhase;
			PresentationCache.Choices = {Choice};
			break;
		}

		Choice.Label = FText::Format(
			LOCTEXT("ChoiceCleanTonightFmt", "Clean For Tomorrow (-{0} Energy)"),
			FText::AsNumber(RuntimeContentAsset != nullptr ? RuntimeContentAsset->Defaults.CleanupEnergyCost : 1));
		Choice.ChoiceType = ESNEChoiceType::CleanStoreForTomorrow;
		Choice.bEnabled = Energy >= (RuntimeContentAsset != nullptr ? RuntimeContentAsset->Defaults.CleanupEnergyCost : 1);
		PresentationCache.Choices.Add(Choice);

		Choice = FSNEChoiceData{};
		Choice.Label = LOCTEXT("ChoiceSkipCleanup", "Leave It For Morning");
		Choice.ChoiceType = ESNEChoiceType::SkipCleanupTomorrow;
		PresentationCache.Choices.Add(Choice);
		break;

	case ESNEDayPhase::RandomEvent:
		PresentationCache.HeaderText = LOCTEXT("HeaderRandomEvent", "Random Event");
		PresentationCache.BodyText = LastEventText;
		Choice.Label = LOCTEXT("ChoiceEndDay", "End Day");
		Choice.ChoiceType = ESNEChoiceType::AdvancePhase;
		PresentationCache.Choices = {Choice};
		break;

	case ESNEDayPhase::DayEnd:
		PresentationCache.HeaderText = LOCTEXT("HeaderDayEnd", "Day Complete");
		PresentationCache.BodyText = LastEventText;
		Choice.Label = FText::Format(LOCTEXT("ChoiceStartNextDayFmt", "Start Day {0}"), FText::AsNumber(DayNumber + 1));
		Choice.ChoiceType = ESNEChoiceType::AdvancePhase;
		PresentationCache.Choices = {Choice};
		Choice = FSNEChoiceData{};
		Choice.Label = LOCTEXT("ChoiceRestartDay", "Restart From Day 1");
		Choice.ChoiceType = ESNEChoiceType::RestartDay;
		PresentationCache.Choices.Add(Choice);
		break;
	}

	ApplySkillGatesToChoices();
	AppendSkillCommentary();
}

int32 USNEDialogueGameSubsystem::GetSkillModifier(ESNESkill Skill) const
{
	// Map raw meter values into a tabletop-style modifier band.
	// Money is far higher-magnitude than the others, so it gets its own scale.
	int32 Raw = 0;
	switch (Skill)
	{
	case ESNESkill::Money:    Raw = Money / 50; break;
	case ESNESkill::Energy:   Raw = Energy / 2;  break;
	case ESNESkill::Sanity:   Raw = Sanity;      break;
	case ESNESkill::Morality: Raw = Morality;    break;
	}

	// Add matured Thought Cabinet bonuses for this skill.
	int32 ThoughtBonus = 0;
	for (const FSNEActiveThought& Active : ActiveThoughts)
	{
		if (!Active.bMatured) continue;
		const USNEThoughtDataAsset* Asset = Active.Thought.LoadSynchronous();
		if (Asset != nullptr && Asset->ApplicableSkill == Skill)
		{
			ThoughtBonus += Asset->MaturedModifier;
		}
	}

	return FMath::Clamp(Raw + ThoughtBonus, -3, 6);
}

void USNEDialogueGameSubsystem::ApplySkillGatesToChoices()
{
	// DE-style: locked options are still visible (greyed out) and labeled with
	// the required skill threshold so the player sees the depth they're missing.
	const UEnum* SkillEnum = StaticEnum<ESNESkill>();
	for (FSNEChoiceData& Choice : PresentationCache.Choices)
	{
		if (!Choice.bHasSkillRequirement) continue;

		int32 CurrentValue = 0;
		switch (Choice.RequiredSkill)
		{
		case ESNESkill::Money:    CurrentValue = Money;    break;
		case ESNESkill::Energy:   CurrentValue = Energy;   break;
		case ESNESkill::Sanity:   CurrentValue = Sanity;   break;
		case ESNESkill::Morality: CurrentValue = Morality; break;
		}

		const bool bSkillPasses = CurrentValue >= Choice.RequiredValue;
		if (!bSkillPasses)
		{
			Choice.bEnabled = false;
		}

		const FText SkillName = SkillEnum != nullptr
			? SkillEnum->GetDisplayNameTextByValue(static_cast<int64>(Choice.RequiredSkill))
			: FText::FromString(TEXT("SKILL"));
		Choice.Label = FText::Format(
			NSLOCTEXT("SNE", "SkillGatedChoiceFmt", "[{0} {1}] {2}"),
			SkillName,
			FText::AsNumber(Choice.RequiredValue),
			Choice.Label);
	}
}

int32 USNEDialogueGameSubsystem::GetThoughtSlots() const
{
	return FMath::Clamp(1 + (Sanity / 3), 1, 5);
}

TArray<FSNEActiveThought> USNEDialogueGameSubsystem::GetActiveThoughts() const
{
	return ActiveThoughts;
}

bool USNEDialogueGameSubsystem::InternalizeThought(USNEThoughtDataAsset* Thought)
{
	if (Thought == nullptr) return false;
	if (ActiveThoughts.Num() >= GetThoughtSlots()) return false;

	const FSoftObjectPath InPath(Thought);
	for (const FSNEActiveThought& Active : ActiveThoughts)
	{
		if (Active.Thought.ToSoftObjectPath() == InPath) return false;
	}

	FSNEActiveThought Entry;
	Entry.Thought = Thought;
	Entry.DaysRemaining = FMath::Max(1, Thought->DaysToInternalize);
	Entry.bMatured = false;
	ActiveThoughts.Add(Entry);

	OnThoughtsChanged.Broadcast();
	return true;
}

bool USNEDialogueGameSubsystem::ForgetThought(USNEThoughtDataAsset* Thought)
{
	if (Thought == nullptr) return false;
	const FSoftObjectPath Target(Thought);
	const int32 Removed = ActiveThoughts.RemoveAll(
		[&Target](const FSNEActiveThought& A){ return A.Thought.ToSoftObjectPath() == Target; });
	if (Removed > 0)
	{
		OnThoughtsChanged.Broadcast();
		return true;
	}
	return false;
}

void USNEDialogueGameSubsystem::TickThoughts()
{
	bool bChanged = false;
	for (FSNEActiveThought& Active : ActiveThoughts)
	{
		if (Active.bMatured) continue;
		Active.DaysRemaining = FMath::Max(0, Active.DaysRemaining - 1);
		if (Active.DaysRemaining <= 0)
		{
			Active.bMatured = true;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		OnThoughtsChanged.Broadcast();
	}
}

void USNEDialogueGameSubsystem::ApplyDailyThoughtPenalties()
{
	for (const FSNEActiveThought& Active : ActiveThoughts)
	{
		if (Active.bMatured) continue;
		const USNEThoughtDataAsset* Asset = Active.Thought.LoadSynchronous();
		if (Asset == nullptr) continue;
		ApplyMeterDelta(Asset->DailyInternalizingPenalty);
	}
}

FSNESkillCheckResult USNEDialogueGameSubsystem::RollSkillCheck(const FSNESkillCheck& Check)
{
	FSNESkillCheckResult Result;
	Result.Skill = Check.Skill;
	Result.DifficultyClass = Check.DifficultyClass;
	Result.ContextLabel = Check.ContextLabel;

	// 2d6 — use the seeded RandomStream so SetRandomSeedForTesting still works.
	const int32 D1 = RandomStream.RandRange(1, 6);
	const int32 D2 = RandomStream.RandRange(1, 6);
	Result.DiceRoll = D1 + D2;
	Result.SkillModifier = GetSkillModifier(Check.Skill);
	Result.Total = Result.DiceRoll + Result.SkillModifier;
	Result.Margin = Result.Total - Result.DifficultyClass;

	// Snake-eyes always fail, boxcars always pass (DE-style critical bands).
	if (Result.DiceRoll == 2)
	{
		Result.bPassed = false;
	}
	else if (Result.DiceRoll == 12)
	{
		Result.bPassed = true;
	}
	else
	{
		Result.bPassed = (Result.Total >= Result.DifficultyClass);
	}

	LastCheckResult = Result;
	bHasLastCheckResult = true;
	return Result;
}

void USNEDialogueGameSubsystem::AppendSkillCommentary()
{
	// Disco-Elysium-style internal voices: each meter whispers a contextual line
	// when its value crosses thresholds. Lines are filtered to current phase so
	// the Morning News doesn't get encounter-only commentary.
	PresentationCache.SkillCommentary.Reset();

	const bool bInEncounter = (CurrentPhase == ESNEDayPhase::MorningShift
		|| CurrentPhase == ESNEDayPhase::EveningShift)
		&& !ActiveEncounter.bResolved
		&& ActiveEncounter.ScenarioIndex != INDEX_NONE;

	auto Push = [this](ESNESkill Skill, const FText& Line)
	{
		FSNESkillLine Entry;
		Entry.Skill = Skill;
		Entry.Line = Line;
		PresentationCache.SkillCommentary.Add(Entry);
	};

	// Surface the most recent skill check exactly once.
	if (bHasLastCheckResult)
	{
		FText CheckLine;
		if (!LastCheckResult.ContextLabel.IsEmpty())
		{
			CheckLine = FText::Format(
				LOCTEXT("VoiceCheckLabeledFmt",
					"{0}: {1} (rolled {2} + {3} = {4} vs DC {5})."),
				LastCheckResult.ContextLabel,
				LastCheckResult.bPassed ? LOCTEXT("CheckPass", "PASS") : LOCTEXT("CheckFail", "FAIL"),
				FText::AsNumber(LastCheckResult.DiceRoll),
				FText::AsNumber(LastCheckResult.SkillModifier),
				FText::AsNumber(LastCheckResult.Total),
				FText::AsNumber(LastCheckResult.DifficultyClass));
		}
		else
		{
			CheckLine = FText::Format(
				LOCTEXT("VoiceCheckUnlabeledFmt",
					"{0} — rolled {1} + {2} = {3} vs DC {4}."),
				LastCheckResult.bPassed ? LOCTEXT("CheckPass", "PASS") : LOCTEXT("CheckFail", "FAIL"),
				FText::AsNumber(LastCheckResult.DiceRoll),
				FText::AsNumber(LastCheckResult.SkillModifier),
				FText::AsNumber(LastCheckResult.Total),
				FText::AsNumber(LastCheckResult.DifficultyClass));
		}
		Push(LastCheckResult.Skill, CheckLine);
		bHasLastCheckResult = false;
	}

	// MONEY voice — the merchant's appetite
	if (bInEncounter)
	{
		if (Money <= 0)
		{
			Push(ESNESkill::Money, LOCTEXT("VoiceMoneyBroke",
				"The till is empty. Every coin matters now. Sell. SELL."));
		}
		else if (Money >= 200)
		{
			Push(ESNESkill::Money, LOCTEXT("VoiceMoneyFat",
				"You can afford a little principle today. A little."));
		}
	}

	// ENERGY voice — the body keeping score
	if (Energy <= 2)
	{
		Push(ESNESkill::Energy, LOCTEXT("VoiceEnergyLow",
			"Your hands are heavy. Investigation is a luxury you can no longer afford."));
	}

	// SANITY voice — the part of you that notices wrong things
	if (bInEncounter)
	{
		if (Sanity <= 0)
		{
			Push(ESNESkill::Sanity, LOCTEXT("VoiceSanityBroken",
				"The walls are listening. The customer's smile has too many teeth."));
		}
		else if (Sanity >= 4)
		{
			Push(ESNESkill::Sanity, LOCTEXT("VoiceSanityHigh",
				"Something about this item is humming under the surface. You can feel it."));
		}
	}

	// MORALITY voice — the conscience that won't shut up
	if (bInEncounter)
	{
		if (Morality <= -3)
		{
			Push(ESNESkill::Morality, LOCTEXT("VoiceMoralityFallen",
				"You stopped flinching weeks ago. Just take their money."));
		}
		else if (Morality >= 3)
		{
			Push(ESNESkill::Morality, LOCTEXT("VoiceMoralityHigh",
				"If something is wrong with this sale, you owe it to them to find out."));
		}
	}

	// MorningNews framing
	if (CurrentPhase == ESNEDayPhase::MorningNews)
	{
		if (Sanity <= 0)
		{
			Push(ESNESkill::Sanity, LOCTEXT("VoiceMorningSanityLow",
				"You don't remember reading this. You're not sure you read it now."));
		}
		if (Morality <= -3)
		{
			Push(ESNESkill::Morality, LOCTEXT("VoiceMorningMoralityLow",
				"Another headline. Another shrug. The shrug used to hurt."));
		}
	}
}

void USNEDialogueGameSubsystem::BuildShiftEncounterPresentation()
{
	PresentationCache.HeaderText = CurrentPhase == ESNEDayPhase::MorningShift
		? LOCTEXT("HeaderMorningShift", "Morning Shift")
		: LOCTEXT("HeaderEveningShift", "Evening Shift");

	if (RuntimeContentAsset == nullptr || RuntimeContentAsset->Customers.Num() == 0)
	{
		PresentationCache.BodyText = LOCTEXT("ShiftNoCustomersConfigured", "No customer scenarios are configured.");
		FSNEChoiceData Choice;
		Choice.Label = LOCTEXT("ShiftNoCustomersContinue", "Continue");
		Choice.ChoiceType = ESNEChoiceType::AdvancePhase;
		PresentationCache.Choices = {Choice};
		ActiveEncounter = FSNEActiveEncounter{};
		ActiveEncounter.bResolved = true;
		return;
	}

	if (ActiveEncounter.ScenarioIndex == INDEX_NONE)
	{
		PresentationCache.BodyText = LOCTEXT("ShiftNoEncounter", "No more highlighted customers in this shift.");
		FSNEChoiceData Choice;
		Choice.Label = LOCTEXT("ShiftNoEncounterContinue", "Continue");
		Choice.ChoiceType = ESNEChoiceType::AdvancePhase;
		PresentationCache.Choices = {Choice};
		ActiveEncounter.bResolved = true;
		return;
	}

	if (!RuntimeContentAsset->Customers.IsValidIndex(ActiveEncounter.ScenarioIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: Invalid active scenario index %d (Customers=%d)."), ActiveEncounter.ScenarioIndex, RuntimeContentAsset->Customers.Num());
		PresentationCache.BodyText = LOCTEXT("ShiftInvalidEncounterData", "Encounter data is missing. Continuing.");
		FSNEChoiceData Choice;
		Choice.Label = LOCTEXT("ShiftInvalidEncounterContinue", "Continue");
		Choice.ChoiceType = ESNEChoiceType::AdvancePhase;
		PresentationCache.Choices = {Choice};
		ActiveEncounter = FSNEActiveEncounter{};
		ActiveEncounter.bResolved = true;
		return;
	}

	const FSNECustomerScenario& Scenario = RuntimeContentAsset->Customers[ActiveEncounter.ScenarioIndex];
	const FSNECustomerVisit* VisitPtr = Scenario.Visits.IsValidIndex(ActiveEncounter.VisitIndex)
		? &Scenario.Visits[ActiveEncounter.VisitIndex]
		: nullptr;
	const FText ItemTitle = ResolveItemDisplayName(GetActiveEncounterItem());
	PresentationCache.CustomerTitle = Scenario.Nickname;
	PresentationCache.ItemTitle = ItemTitle;
	PresentationCache.VisibleClues = ActiveEncounter.VisibleClues;
	PresentationCache.SkillAttributedClues = ActiveEncounter.SkillAttributedClues;

	const int32 ResolvedCount = CurrentPhase == ESNEDayPhase::MorningShift ? MorningResolvedCount : EveningResolvedCount;
	const int32 TargetCount = GetRequiredEncountersForCurrentShift();
	const int32 DisplayIndex = FMath::Clamp(ResolvedCount + 1, 1, TargetCount);

	if (ActiveEncounter.bResolved)
	{
		PresentationCache.BodyText = LastEventText;
		FSNEChoiceData ContinueChoice;
		ContinueChoice.Label = FText::Format(
			LOCTEXT("ShiftContinueFmt", "Continue ({0}/{1})"),
			FText::AsNumber(DisplayIndex),
			FText::AsNumber(TargetCount));
		ContinueChoice.ChoiceType = ESNEChoiceType::AdvancePhase;
		PresentationCache.Choices = {ContinueChoice};
		return;
	}

	FText InvestigateHint = LOCTEXT("InvestigateHintUnknown", "Investigate may reveal 2-3 clues, but certainty is never guaranteed.");
	if (ActiveEncounter.bInvestigated && ActiveEncounter.VisibleClues.Num() > 0)
	{
		InvestigateHint = LOCTEXT("InvestigateHintDone", "You have investigated. Use the clues and decide.");
	}

	const FText OpeningDialogue = VisitPtr != nullptr ? VisitPtr->OpeningDialogue : FText::GetEmpty();
	PresentationCache.BodyText = FText::Format(
		LOCTEXT("ShiftBodyFmt", "Customer {0}/{1}: {2}\nItem Requested: {3}\n\n{4}\n\n{5}"),
		FText::AsNumber(DisplayIndex),
		FText::AsNumber(TargetCount),
		Scenario.Nickname,
		ItemTitle,
		OpeningDialogue,
		InvestigateHint);

	FSNEChoiceData Choice;
	Choice.Label = FText::Format(
		LOCTEXT("InvestigateChoiceFmt", "Investigate (-{0} Energy)"),
		FText::AsNumber(RuntimeContentAsset->Defaults.InvestigateEnergyCost));
	Choice.ChoiceType = ESNEChoiceType::Investigate;
	Choice.bEnabled = !ActiveEncounter.bInvestigated && Energy >= RuntimeContentAsset->Defaults.InvestigateEnergyCost;
	PresentationCache.Choices.Add(Choice);

	Choice = FSNEChoiceData{};
	Choice.Label = LOCTEXT("SellChoice", "Sell Item");
	Choice.ChoiceType = ESNEChoiceType::Sell;
	PresentationCache.Choices.Add(Choice);

	Choice = FSNEChoiceData{};
	Choice.Label = LOCTEXT("NoSellChoice", "Do Not Sell");
	Choice.ChoiceType = ESNEChoiceType::NoSell;
	PresentationCache.Choices.Add(Choice);
}

void USNEDialogueGameSubsystem::StartNextEncounterIfNeeded()
{
	if (RuntimeContentAsset == nullptr)
	{
		return;
	}

	const int32 TargetCount = GetRequiredEncountersForCurrentShift();
	int32& ResolvedCount = CurrentPhase == ESNEDayPhase::MorningShift ? MorningResolvedCount : EveningResolvedCount;

	if (ResolvedCount >= TargetCount)
	{
		ActiveEncounter = FSNEActiveEncounter{};
		ActiveEncounter.bResolved = true;
		return;
	}

	if (RuntimeContentAsset->Customers.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: No customers configured; skipping remaining shift encounters."));
		ResolvedCount = TargetCount;
		ActiveEncounter = FSNEActiveEncounter{};
		ActiveEncounter.bResolved = true;
		return;
	}

	if (CurrentEncounterOrderIndex >= DailyCustomerOrder.Num())
	{
		BuildDailyCustomerOrder();
		CurrentEncounterOrderIndex = 0;
	}

	int32 ScenarioIndex = INDEX_NONE;
	if (DailyCustomerOrder.IsValidIndex(CurrentEncounterOrderIndex))
	{
		const int32 OrderedIndex = DailyCustomerOrder[CurrentEncounterOrderIndex];
		if (RuntimeContentAsset->Customers.IsValidIndex(OrderedIndex))
		{
			ScenarioIndex = OrderedIndex;
		}
	}
	if (ScenarioIndex == INDEX_NONE && RuntimeContentAsset->Customers.Num() > 0)
	{
		ScenarioIndex = RandomStream.RandRange(0, RuntimeContentAsset->Customers.Num() - 1);
	}
	++CurrentEncounterOrderIndex;

	if (!RuntimeContentAsset->Customers.IsValidIndex(ScenarioIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: Could not select a valid customer scenario. Customers=%d OrderSize=%d."),
			RuntimeContentAsset->Customers.Num(),
			DailyCustomerOrder.Num());
		ResolvedCount = TargetCount;
		ActiveEncounter = FSNEActiveEncounter{};
		ActiveEncounter.bResolved = true;
		return;
	}

	const FSNECustomerScenario& Scenario = RuntimeContentAsset->Customers[ScenarioIndex];
	const FSNECustomerHistory& History = CustomerHistories.FindOrAdd(Scenario.Id);
	const int32 VisitIndex = PickEligibleVisitIndex(Scenario, History);

	if (VisitIndex == INDEX_NONE || !Scenario.Visits.IsValidIndex(VisitIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: No eligible visit for scenario %s; skipping."), *Scenario.Id.ToString());
		ResolvedCount = FMath::Min(ResolvedCount + 1, TargetCount);
		ActiveEncounter = FSNEActiveEncounter{};
		ActiveEncounter.bResolved = true;
		return;
	}

	const FSNECustomerVisit& Visit = Scenario.Visits[VisitIndex];

	ActiveEncounter = FSNEActiveEncounter{};
	ActiveEncounter.ScenarioIndex = ScenarioIndex;
	ActiveEncounter.VisitIndex = VisitIndex;
	ActiveEncounter.VisitCountAtSelection = History.VisitCount;
	ActiveEncounter.SelectedItemPoolIndex = Visit.RequestedItemPool.Num() > 0
		? RandomStream.RandRange(0, Visit.RequestedItemPool.Num() - 1)
		: INDEX_NONE;
	const float GoodIntentChance = SNESubsystemInternal::ComputeGoodIntentChance(
		Visit.GoodIntentChance,
		RuntimeContentAsset->Defaults,
		Sanity,
		Morality,
		Money,
		bStoreCleanForTomorrow);
	ActiveEncounter.Intent = RandomStream.FRand() < GoodIntentChance ? ESNECustomerIntent::Good : ESNECustomerIntent::Bad;
	ActiveEncounter.bInvestigated = false;
	ActiveEncounter.bResolved = false;
	ActiveEncounter.VisibleClues.Reset();
}

void USNEDialogueGameSubsystem::FinalizeCurrentEncounter(const bool bSold)
{
	if (RuntimeContentAsset == nullptr || ActiveEncounter.ScenarioIndex == INDEX_NONE)
	{
		return;
	}

	if (!RuntimeContentAsset->Customers.IsValidIndex(ActiveEncounter.ScenarioIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: FinalizeCurrentEncounter aborted due to invalid scenario index %d."), ActiveEncounter.ScenarioIndex);
		LastEventText = LOCTEXT("FinalizeInvalidEncounter", "Encounter data was missing. Continuing shift.");
		ActiveEncounter = FSNEActiveEncounter{};
		ActiveEncounter.bResolved = true;
		int32& ResolvedCount = CurrentPhase == ESNEDayPhase::MorningShift ? MorningResolvedCount : EveningResolvedCount;
		const int32 TargetCount = GetRequiredEncountersForCurrentShift();
		ResolvedCount = FMath::Clamp(ResolvedCount + 1, 0, TargetCount);
		return;
	}

	const FSNECustomerScenario& Scenario = RuntimeContentAsset->Customers[ActiveEncounter.ScenarioIndex];
	if (!Scenario.Visits.IsValidIndex(ActiveEncounter.VisitIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: FinalizeCurrentEncounter aborted due to invalid visit index %d on scenario %s."), ActiveEncounter.VisitIndex, *Scenario.Id.ToString());
		LastEventText = LOCTEXT("FinalizeInvalidVisit", "Visit data was missing. Continuing shift.");
		ActiveEncounter = FSNEActiveEncounter{};
		ActiveEncounter.bResolved = true;
		int32& ResolvedCount = CurrentPhase == ESNEDayPhase::MorningShift ? MorningResolvedCount : EveningResolvedCount;
		const int32 TargetCount = GetRequiredEncountersForCurrentShift();
		ResolvedCount = FMath::Clamp(ResolvedCount + 1, 0, TargetCount);
		return;
	}
	const FSNECustomerVisit& Visit = Scenario.Visits[ActiveEncounter.VisitIndex];
	const FSNEOutcomeData& Outcome = SelectOutcome(Visit, bSold, ActiveEncounter.Intent);
	const USNEItemDataAsset* ActiveItem = GetActiveEncounterItem();

	int32 TipAmount = 0;
	if (bSold)
	{
		const int32 SaleValue = GetSaleValue(ActiveItem);
		Money += SaleValue;

		// Tip is now a MONEY skill check instead of a flat probability.
		// DC is derived from current effective TipChance so existing content tuning
		// continues to work: 50% TipChance ~= DC 9 (medium), 5% ~= DC 14 (hard).
		const float EffectiveTipChance = FMath::Clamp(TipChance * Scenario.TipChanceMultiplier, 0.0f, 1.0f);
		FSNESkillCheck TipCheck;
		TipCheck.Skill = ESNESkill::Money;
		TipCheck.DifficultyClass = FMath::Clamp(
			14 - FMath::RoundToInt(EffectiveTipChance * 10.0f),
			6, 18);
		TipCheck.bRedCheck = true;
		TipCheck.ContextLabel = LOCTEXT("TipCheckLabel", "Read the customer for a tip");
		const FSNESkillCheckResult TipResult = RollSkillCheck(TipCheck);
		if (TipResult.bPassed)
		{
			// Margin gives a small payout bonus on big wins.
			const float MarginBonus = FMath::Clamp(TipResult.Margin * 0.005f, 0.0f, 0.05f);
			TipAmount = FMath::RoundToInt(static_cast<float>(SaleValue) * (0.05f + MarginBonus));
			Money += TipAmount;
		}
	}

	FSNEMeterDelta ImmediateDelta = Outcome.ImmediateDelta;
	FSNEMeterDelta DeferredEthicsDelta;
	DeferredEthicsDelta.Sanity = ImmediateDelta.Sanity;
	DeferredEthicsDelta.Morality = ImmediateDelta.Morality;
	ImmediateDelta.Sanity = 0;
	ImmediateDelta.Morality = 0;

	ApplyMeterDelta(ImmediateDelta);
	const bool bQueuedBaseDelayed = AppendDelayedOutcome(Outcome);

	if (DeferredEthicsDelta.Sanity != 0 || DeferredEthicsDelta.Morality != 0)
	{
		const FText DeferredNewsText = SNESubsystemInternal::BuildDeferredEthicsNewsText(
			Scenario,
			ResolveItemDisplayName(ActiveItem),
			bSold,
			ActiveEncounter.Intent,
			DeferredEthicsDelta);

		if (bQueuedBaseDelayed && PendingDelayedOutcomes.Num() > 0)
		{
			FSNEDelayedOutcomeEntry& LatestEntry = PendingDelayedOutcomes.Last();
			LatestEntry.LaterDelta.Sanity += DeferredEthicsDelta.Sanity;
			LatestEntry.LaterDelta.Morality += DeferredEthicsDelta.Morality;

			if (!LatestEntry.LaterText.IsEmpty())
			{
				LatestEntry.LaterText = FText::FromString(FString::Printf(
					TEXT("%s\n\n%s"),
					*DeferredNewsText.ToString(),
					*LatestEntry.LaterText.ToString()));
			}
			else
			{
				LatestEntry.LaterText = DeferredNewsText;
			}
		}
		else
		{
			FSNEDelayedOutcomeEntry DeferredEntry;
			DeferredEntry.LaterText = DeferredNewsText;
			DeferredEntry.LaterDelta = DeferredEthicsDelta;
			PendingDelayedOutcomes.Add(DeferredEntry);
		}
	}

	ActiveEncounter.bResolved = true;
	int32& ResolvedCount = CurrentPhase == ESNEDayPhase::MorningShift ? MorningResolvedCount : EveningResolvedCount;
	++ResolvedCount;

	// Update persistent customer history so later visits can gate on it.
	FSNECustomerHistory& History = CustomerHistories.FindOrAdd(Scenario.Id);
	History.VisitCount = ActiveEncounter.VisitCountAtSelection + 1;
	History.LastVisitDay = DayNumber;
	History.LastDecision = bSold ? ESNEPreviousDecision::Sold : ESNEPreviousDecision::NotSold;
	History.LastIntent = ActiveEncounter.Intent;
	if (!Visit.VisitId.IsNone() && !History.CompletedVisitIds.Contains(Visit.VisitId))
	{
		History.CompletedVisitIds.Add(Visit.VisitId);
	}

	if (TipAmount > 0)
	{
		LastEventText = FText::Format(
			LOCTEXT("OutcomeWithTipFmt", "{0}\n\nTip received: +{1} MNT"),
			Outcome.ImmediateText,
			FText::AsNumber(TipAmount));
	}
	else
	{
		LastEventText = Outcome.ImmediateText;
	}
}

bool USNEDialogueGameSubsystem::AppendDelayedOutcome(const FSNEOutcomeData& Outcome)
{
	const bool bHasLaterText = !Outcome.LaterText.IsEmpty();
	if (!bHasLaterText && !SNESubsystemInternal::HasMeaningfulDelta(Outcome.LaterDelta))
	{
		return false;
	}

	FSNEDelayedOutcomeEntry Entry;
	Entry.LaterText = Outcome.LaterText;
	Entry.LaterDelta = Outcome.LaterDelta;
	PendingDelayedOutcomes.Add(Entry);
	return true;
}

const FSNEOutcomeData& USNEDialogueGameSubsystem::SelectOutcome(const FSNECustomerVisit& Visit, const bool bSell, const ESNECustomerIntent Intent) const
{
	if (bSell)
	{
		return Intent == ESNECustomerIntent::Good
			? Visit.SellGoodIntentOutcome
			: Visit.SellBadIntentOutcome;
	}

	return Intent == ESNECustomerIntent::Good
		? Visit.NoSellGoodIntentOutcome
		: Visit.NoSellBadIntentOutcome;
}

bool USNEDialogueGameSubsystem::IsVisitEligible(const FSNECustomerVisit& Visit, const FSNECustomerScenario& Scenario, const FSNECustomerHistory& History) const
{
	const FSNEVisitConditions& C = Visit.Conditions;

	if (History.VisitCount < C.MinVisitCount) return false;
	if (C.MaxVisitCount >= 0 && History.VisitCount > C.MaxVisitCount) return false;
	if (DayNumber < C.MinDayNumber) return false;
	if (C.MaxDayNumber >= 0 && DayNumber > C.MaxDayNumber) return false;
	if (Morality < C.MinMorality || Morality > C.MaxMorality) return false;
	if (Sanity < C.MinSanity || Sanity > C.MaxSanity) return false;

	for (const FName& RequiredId : C.RequiresPreviousVisitIds)
	{
		if (RequiredId.IsNone()) continue;
		if (!History.CompletedVisitIds.Contains(RequiredId)) return false;
	}

	for (const FName& BlockedId : C.BlockedByPreviousVisitIds)
	{
		if (BlockedId.IsNone()) continue;
		if (History.CompletedVisitIds.Contains(BlockedId)) return false;
	}

	switch (C.RequiredLastDecision)
	{
	case ESNEPreviousDecision::Any:
		break;
	case ESNEPreviousDecision::NeverMet:
		if (History.LastDecision != ESNEPreviousDecision::NeverMet) return false;
		break;
	case ESNEPreviousDecision::Sold:
		if (History.LastDecision != ESNEPreviousDecision::Sold) return false;
		break;
	case ESNEPreviousDecision::NotSold:
		if (History.LastDecision != ESNEPreviousDecision::NotSold) return false;
		break;
	}

	if (C.RequiredLastIntent == ESNEIntentFilter::Good && (History.LastDecision == ESNEPreviousDecision::NeverMet || History.LastIntent != ESNECustomerIntent::Good)) return false;
	if (C.RequiredLastIntent == ESNEIntentFilter::Bad && (History.LastDecision == ESNEPreviousDecision::NeverMet || History.LastIntent != ESNECustomerIntent::Bad)) return false;

	return true;
}

int32 USNEDialogueGameSubsystem::PickEligibleVisitIndex(const FSNECustomerScenario& Scenario, const FSNECustomerHistory& History) const
{
	int32 BestIndex = INDEX_NONE;
	int32 BestPriority = MIN_int32;

	for (int32 Index = 0; Index < Scenario.Visits.Num(); ++Index)
	{
		const FSNECustomerVisit& Visit = Scenario.Visits[Index];
		if (!IsVisitEligible(Visit, Scenario, History)) continue;

		if (BestIndex == INDEX_NONE || Visit.Priority > BestPriority)
		{
			BestIndex = Index;
			BestPriority = Visit.Priority;
		}
	}

	// Arc-end fallback: if no visit matched AND the customer has been seen, apply ArcEndBehavior.
	if (BestIndex == INDEX_NONE && History.VisitCount > 0 && Scenario.Visits.Num() > 0)
	{
		if (Scenario.ArcEndBehavior == ESNEArcEndBehavior::LoopLast)
		{
			return Scenario.Visits.Num() - 1;
		}
	}

	return BestIndex;
}

bool USNEDialogueGameSubsystem::IsCustomerEligibleToday(const FSNECustomerScenario& Scenario, const FSNECustomerHistory& History, const bool bAlreadyDrawnToday) const
{
	if (bAlreadyDrawnToday && !Scenario.bAllowMultipleVisitsPerDay) return false;

	if (History.VisitCount > 0 && Scenario.Visits.Num() > 0)
	{
		const int32 Completed = History.CompletedVisitIds.Num();
		const bool bArcExhausted = Completed >= Scenario.Visits.Num();
		if (bArcExhausted)
		{
			switch (Scenario.ArcEndBehavior)
			{
			case ESNEArcEndBehavior::Remove:
			case ESNEArcEndBehavior::Silent:
				return false;
			case ESNEArcEndBehavior::LoopLast:
				return true;
			}
		}
	}

	return PickEligibleVisitIndex(Scenario, History) != INDEX_NONE;
}

int32 USNEDialogueGameSubsystem::GetRequiredEncountersForCurrentShift() const
{
	if (RuntimeContentAsset == nullptr)
	{
		return 0;
	}

	return CurrentPhase == ESNEDayPhase::MorningShift
		? RuntimeContentAsset->Defaults.MorningCustomerCount
		: RuntimeContentAsset->Defaults.EveningCustomerCount;
}

const USNEItemDataAsset* USNEDialogueGameSubsystem::GetActiveEncounterItem() const
{
	if (RuntimeContentAsset == nullptr || !RuntimeContentAsset->Customers.IsValidIndex(ActiveEncounter.ScenarioIndex))
	{
		return nullptr;
	}
	const FSNECustomerScenario& Scenario = RuntimeContentAsset->Customers[ActiveEncounter.ScenarioIndex];
	if (!Scenario.Visits.IsValidIndex(ActiveEncounter.VisitIndex))
	{
		return nullptr;
	}
	const FSNECustomerVisit& Visit = Scenario.Visits[ActiveEncounter.VisitIndex];
	if (!Visit.RequestedItemPool.IsValidIndex(ActiveEncounter.SelectedItemPoolIndex))
	{
		return nullptr;
	}
	return Visit.RequestedItemPool[ActiveEncounter.SelectedItemPoolIndex].LoadSynchronous();
}

int32 USNEDialogueGameSubsystem::GetSaleValue(const USNEItemDataAsset* Item) const
{
	return Item != nullptr ? FMath::Max(0, Item->BaseSaleValue) : 0;
}

FText USNEDialogueGameSubsystem::ResolveItemDisplayName(const USNEItemDataAsset* Item)
{
	if (Item != nullptr && !Item->DisplayName.IsEmpty())
	{
		return Item->DisplayName;
	}
	return NSLOCTEXT("SNEDialogueSubsystem", "UnknownItemFallback", "Unspecified item");
}

void USNEDialogueGameSubsystem::BuildDailyCustomerOrder()
{
	EnsureContentLoaded();
	DailyCustomerOrder.Reset();
	if (RuntimeContentAsset == nullptr || RuntimeContentAsset->Customers.Num() == 0)
	{
		return;
	}

	const int32 NeededCount = RuntimeContentAsset->Defaults.MorningCustomerCount + RuntimeContentAsset->Defaults.EveningCustomerCount;

	// Start with customers whose arc + conditions let them appear at least once today.
	TArray<int32> EligibleIndices;
	TArray<int32> RepeaterIndices; // subset of eligible that allow multi-draws
	for (int32 Index = 0; Index < RuntimeContentAsset->Customers.Num(); ++Index)
	{
		const FSNECustomerScenario& Scenario = RuntimeContentAsset->Customers[Index];
		const FSNECustomerHistory& History = CustomerHistories.FindOrAdd(Scenario.Id);
		if (!IsCustomerEligibleToday(Scenario, History, /*bAlreadyDrawnToday*/ false)) continue;

		EligibleIndices.Add(Index);
		if (Scenario.bAllowMultipleVisitsPerDay)
		{
			RepeaterIndices.Add(Index);
		}
	}

	if (EligibleIndices.Num() == 0) return;

	// Shuffle the unique-draw pool.
	for (int32 Index = 0; Index < EligibleIndices.Num(); ++Index)
	{
		const int32 SwapIndex = RandomStream.RandRange(Index, EligibleIndices.Num() - 1);
		EligibleIndices.Swap(Index, SwapIndex);
	}

	TSet<int32> DrawnThisDay;
	for (int32 Pick = 0; Pick < NeededCount; ++Pick)
	{
		int32 ChosenIndex = INDEX_NONE;

		// First pass: unseen-today customers.
		for (const int32 Candidate : EligibleIndices)
		{
			if (!DrawnThisDay.Contains(Candidate))
			{
				ChosenIndex = Candidate;
				break;
			}
		}

		// If every eligible has already been drawn today, only customers flagged as repeaters can fill the rest.
		if (ChosenIndex == INDEX_NONE)
		{
			if (RepeaterIndices.Num() == 0) break;
			ChosenIndex = RepeaterIndices[RandomStream.RandRange(0, RepeaterIndices.Num() - 1)];
		}

		DailyCustomerOrder.Add(ChosenIndex);
		DrawnThisDay.Add(ChosenIndex);
	}
}

void USNEDialogueGameSubsystem::BroadcastPresentation()
{
	OnPresentationChanged.Broadcast();
}

FString USNEDialogueGameSubsystem::MakeMeterSummary() const
{
	return FString::Printf(
		TEXT("Money: %d MNT  |  Energy: %d  |  Sanity: %d  |  Morality: %d  |  Tip: %d%%"),
		Money,
		Energy,
		Sanity,
		Morality,
		FMath::RoundToInt(TipChance * 100.0f));
}

void USNEDialogueGameSubsystem::ApplyActionCosts(const int32 EnergyCost, const int32 MoneyCost)
{
	if (RuntimeContentAsset == nullptr)
	{
		return;
	}

	Energy = FMath::Clamp(Energy - EnergyCost, 0, RuntimeContentAsset->Defaults.MaxEnergy);
	Money -= MoneyCost;
}

void USNEDialogueGameSubsystem::ApplyRandomEventIfNeeded()
{
	EnsureContentLoaded();
	if (RuntimeContentAsset == nullptr || bRandomEventApplied)
	{
		return;
	}

	if (RuntimeContentAsset->RandomEvents.Num() == 0)
	{
		LastEventText = LOCTEXT("NoRandomEvents", "No special event tonight.");
		bRandomEventApplied = true;
		return;
	}

	const int32 EventIndex = RandomStream.RandRange(0, RuntimeContentAsset->RandomEvents.Num() - 1);
	const FSNERandomEvent& Event = RuntimeContentAsset->RandomEvents[EventIndex];
	ApplyMeterDelta(Event.ResultDelta);
	LastEventText = Event.EventText;
	bRandomEventApplied = true;
}

const FSNEPrepAction* USNEDialogueGameSubsystem::FindPrepAction(const TArray<FSNEPrepAction>& ActionPool, const FName ActionId) const
{
	for (const FSNEPrepAction& Action : ActionPool)
	{
		if (Action.ActionId == ActionId)
		{
			return &Action;
		}
	}
	return nullptr;
}

const FSNELunchOption* USNEDialogueGameSubsystem::FindLunchOption(const FName OptionId) const
{
	if (RuntimeContentAsset == nullptr)
	{
		return nullptr;
	}

	for (const FSNELunchOption& Option : RuntimeContentAsset->LunchOptions)
	{
		if (Option.OptionId == OptionId)
		{
			return &Option;
		}
	}
	return nullptr;
}

// ============================================================================
// Debug helpers
// ============================================================================

void USNEDialogueGameSubsystem::DebugSetMeters(int32 InMoney, int32 InEnergy, int32 InSanity, int32 InMorality)
{
	EnsureContentLoaded();
	const int32 MaxEnergyClamp = RuntimeContentAsset != nullptr ? RuntimeContentAsset->Defaults.MaxEnergy : 8;
	Money = InMoney;
	Energy = FMath::Clamp(InEnergy, 0, FMath::Max(1, MaxEnergyClamp));
	Sanity = InSanity;
	Morality = InMorality;
	RebuildPresentation();
	BroadcastPresentation();
}

bool USNEDialogueGameSubsystem::DebugForceEncounter(const FName CustomerId, const FName VisitId)
{
	EnsureContentLoaded();
	if (RuntimeContentAsset == nullptr) return false;

	int32 ScenarioIdx = INDEX_NONE;
	for (int32 Idx = 0; Idx < RuntimeContentAsset->Customers.Num(); ++Idx)
	{
		if (RuntimeContentAsset->Customers[Idx].Id == CustomerId)
		{
			ScenarioIdx = Idx;
			break;
		}
	}
	if (ScenarioIdx == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: DebugForceEncounter could not find customer '%s'."), *CustomerId.ToString());
		return false;
	}

	const FSNECustomerScenario& Scenario = RuntimeContentAsset->Customers[ScenarioIdx];
	int32 VisitIdx = INDEX_NONE;
	for (int32 Idx = 0; Idx < Scenario.Visits.Num(); ++Idx)
	{
		if (Scenario.Visits[Idx].VisitId == VisitId)
		{
			VisitIdx = Idx;
			break;
		}
	}
	if (VisitIdx == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: DebugForceEncounter could not find visit '%s' on customer '%s'."), *VisitId.ToString(), *CustomerId.ToString());
		return false;
	}

	if (CurrentPhase != ESNEDayPhase::MorningShift && CurrentPhase != ESNEDayPhase::EveningShift)
	{
		CurrentPhase = ESNEDayPhase::MorningShift;
	}

	const FSNECustomerVisit& Visit = Scenario.Visits[VisitIdx];
	const FSNECustomerHistory& History = CustomerHistories.FindOrAdd(Scenario.Id);

	ActiveEncounter = FSNEActiveEncounter{};
	ActiveEncounter.ScenarioIndex = ScenarioIdx;
	ActiveEncounter.VisitIndex = VisitIdx;
	ActiveEncounter.VisitCountAtSelection = History.VisitCount;
	ActiveEncounter.SelectedItemPoolIndex = Visit.RequestedItemPool.Num() > 0
		? RandomStream.RandRange(0, Visit.RequestedItemPool.Num() - 1)
		: INDEX_NONE;
	ActiveEncounter.Intent = RandomStream.FRand() < FMath::Clamp(Visit.GoodIntentChance, 0.05f, 0.95f) ? ESNECustomerIntent::Good : ESNECustomerIntent::Bad;
	ActiveEncounter.bResolved = false;
	ActiveEncounter.bInvestigated = false;

	RebuildPresentation();
	BroadcastPresentation();
	return true;
}

void USNEDialogueGameSubsystem::DebugJumpToPhase(const ESNEDayPhase Phase)
{
	EnterPhase(Phase);
}

void USNEDialogueGameSubsystem::DebugClearCustomerHistories()
{
	CustomerHistories.Reset();
}

int32 USNEDialogueGameSubsystem::DebugGetCustomerVisitCount(const FName CustomerId) const
{
	if (const FSNECustomerHistory* H = CustomerHistories.Find(CustomerId))
	{
		return H->VisitCount;
	}
	return 0;
}

// ============================================================================
// Save / Load (JSON)
// ============================================================================

namespace SNESaveInternal
{
	static FString GetSaveDir()
	{
		return FPaths::ProjectSavedDir() / TEXT("SNESaves");
	}

	static FString GetSaveFilePath(const FString& SlotName)
	{
		const FString SanitizedSlot = SlotName.IsEmpty() ? TEXT("autosave") : SlotName;
		return GetSaveDir() / (SanitizedSlot + TEXT(".sav"));
	}

	static TSharedPtr<FJsonObject> DeltaToJson(const FSNEMeterDelta& D)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("Money"), D.Money);
		Obj->SetNumberField(TEXT("Energy"), D.Energy);
		Obj->SetNumberField(TEXT("Sanity"), D.Sanity);
		Obj->SetNumberField(TEXT("Morality"), D.Morality);
		Obj->SetNumberField(TEXT("TipChance"), D.TipChance);
		return Obj;
	}

	static void JsonToDelta(const TSharedPtr<FJsonObject>& Obj, FSNEMeterDelta& OutDelta)
	{
		if (!Obj.IsValid()) return;
		OutDelta.Money = Obj->GetIntegerField(TEXT("Money"));
		OutDelta.Energy = Obj->GetIntegerField(TEXT("Energy"));
		OutDelta.Sanity = Obj->GetIntegerField(TEXT("Sanity"));
		OutDelta.Morality = Obj->GetIntegerField(TEXT("Morality"));
		OutDelta.TipChance = static_cast<float>(Obj->GetNumberField(TEXT("TipChance")));
	}
}

bool USNEDialogueGameSubsystem::SaveToSlot(const FString& SlotName)
{
	using namespace SNESaveInternal;

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("Version"), 1);
	Root->SetNumberField(TEXT("DayNumber"), DayNumber);
	Root->SetNumberField(TEXT("CurrentPhase"), static_cast<int32>(CurrentPhase));
	Root->SetNumberField(TEXT("Money"), Money);
	Root->SetNumberField(TEXT("Energy"), Energy);
	Root->SetNumberField(TEXT("Sanity"), Sanity);
	Root->SetNumberField(TEXT("Morality"), Morality);
	Root->SetNumberField(TEXT("TipChance"), TipChance);
	Root->SetBoolField(TEXT("StoreCleanForTomorrow"), bStoreCleanForTomorrow);
	Root->SetNumberField(TEXT("MorningResolvedCount"), MorningResolvedCount);
	Root->SetNumberField(TEXT("EveningResolvedCount"), EveningResolvedCount);
	Root->SetNumberField(TEXT("CurrentEncounterOrderIndex"), CurrentEncounterOrderIndex);
	Root->SetBoolField(TEXT("MorningPrepDone"), bMorningPrepDone);
	Root->SetBoolField(TEXT("LunchDone"), bLunchDone);
	Root->SetBoolField(TEXT("NightPrepDone"), bNightPrepDone);
	Root->SetBoolField(TEXT("ClosingDone"), bClosingDone);
	Root->SetBoolField(TEXT("RandomEventApplied"), bRandomEventApplied);
	Root->SetBoolField(TEXT("HasStartedDay"), bHasStartedDay);
	Root->SetStringField(TEXT("LastEventText"), LastEventText.ToString());

	// DailyCustomerOrder
	{
		TArray<TSharedPtr<FJsonValue>> OrderArr;
		for (int32 Idx : DailyCustomerOrder)
		{
			OrderArr.Add(MakeShared<FJsonValueNumber>(Idx));
		}
		Root->SetArrayField(TEXT("DailyCustomerOrder"), OrderArr);
	}

	// PendingDelayedOutcomes
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FSNEDelayedOutcomeEntry& Entry : PendingDelayedOutcomes)
		{
			TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
			E->SetStringField(TEXT("LaterText"), Entry.LaterText.ToString());
			E->SetObjectField(TEXT("LaterDelta"), DeltaToJson(Entry.LaterDelta));
			Arr.Add(MakeShared<FJsonValueObject>(E));
		}
		Root->SetArrayField(TEXT("PendingDelayedOutcomes"), Arr);
	}

	// CustomerHistories
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const TPair<FName, FSNECustomerHistory>& Pair : CustomerHistories)
		{
			TSharedPtr<FJsonObject> H = MakeShared<FJsonObject>();
			H->SetStringField(TEXT("CustomerId"), Pair.Key.ToString());
			H->SetNumberField(TEXT("VisitCount"), Pair.Value.VisitCount);
			H->SetNumberField(TEXT("LastVisitDay"), Pair.Value.LastVisitDay);
			H->SetNumberField(TEXT("LastDecision"), static_cast<int32>(Pair.Value.LastDecision));
			H->SetNumberField(TEXT("LastIntent"), static_cast<int32>(Pair.Value.LastIntent));
			TArray<TSharedPtr<FJsonValue>> CompletedArr;
			for (const FName& Id : Pair.Value.CompletedVisitIds)
			{
				CompletedArr.Add(MakeShared<FJsonValueString>(Id.ToString()));
			}
			H->SetArrayField(TEXT("CompletedVisitIds"), CompletedArr);
			Arr.Add(MakeShared<FJsonValueObject>(H));
		}
		Root->SetArrayField(TEXT("CustomerHistories"), Arr);
	}

	// ActiveThoughts (Thought Cabinet)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FSNEActiveThought& A : ActiveThoughts)
		{
			TSharedPtr<FJsonObject> T = MakeShared<FJsonObject>();
			T->SetStringField(TEXT("ThoughtPath"), A.Thought.ToSoftObjectPath().ToString());
			T->SetNumberField(TEXT("DaysRemaining"), A.DaysRemaining);
			T->SetBoolField(TEXT("Matured"), A.bMatured);
			Arr.Add(MakeShared<FJsonValueObject>(T));
		}
		Root->SetArrayField(TEXT("ActiveThoughts"), Arr);
	}

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: SaveToSlot failed to serialize."));
		return false;
	}

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	const FString Dir = GetSaveDir();
	if (!PF.DirectoryExists(*Dir))
	{
		PF.CreateDirectoryTree(*Dir);
	}
	const FString Path = GetSaveFilePath(SlotName);
	if (!FFileHelper::SaveStringToFile(Out, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: SaveToSlot failed to write '%s'."), *Path);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("SNE: Saved game to '%s'."), *Path);
	return true;
}

bool USNEDialogueGameSubsystem::LoadFromSlot(const FString& SlotName)
{
	using namespace SNESaveInternal;
	EnsureContentLoaded();

	const FString Path = GetSaveFilePath(SlotName);
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: LoadFromSlot could not read '%s'."), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: LoadFromSlot could not parse '%s'."), *Path);
		return false;
	}

	DayNumber = Root->GetIntegerField(TEXT("DayNumber"));
	CurrentPhase = static_cast<ESNEDayPhase>(Root->GetIntegerField(TEXT("CurrentPhase")));
	Money = Root->GetIntegerField(TEXT("Money"));
	Energy = Root->GetIntegerField(TEXT("Energy"));
	Sanity = Root->GetIntegerField(TEXT("Sanity"));
	Morality = Root->GetIntegerField(TEXT("Morality"));
	TipChance = static_cast<float>(Root->GetNumberField(TEXT("TipChance")));
	bStoreCleanForTomorrow = Root->GetBoolField(TEXT("StoreCleanForTomorrow"));
	MorningResolvedCount = Root->GetIntegerField(TEXT("MorningResolvedCount"));
	EveningResolvedCount = Root->GetIntegerField(TEXT("EveningResolvedCount"));
	CurrentEncounterOrderIndex = Root->GetIntegerField(TEXT("CurrentEncounterOrderIndex"));
	bMorningPrepDone = Root->GetBoolField(TEXT("MorningPrepDone"));
	bLunchDone = Root->GetBoolField(TEXT("LunchDone"));
	bNightPrepDone = Root->GetBoolField(TEXT("NightPrepDone"));
	bClosingDone = Root->GetBoolField(TEXT("ClosingDone"));
	bRandomEventApplied = Root->GetBoolField(TEXT("RandomEventApplied"));
	bHasStartedDay = Root->GetBoolField(TEXT("HasStartedDay"));
	LastEventText = FText::FromString(Root->GetStringField(TEXT("LastEventText")));

	DailyCustomerOrder.Reset();
	const TArray<TSharedPtr<FJsonValue>>* OrderArr = nullptr;
	if (Root->TryGetArrayField(TEXT("DailyCustomerOrder"), OrderArr))
	{
		for (const TSharedPtr<FJsonValue>& V : *OrderArr)
		{
			DailyCustomerOrder.Add(static_cast<int32>(V->AsNumber()));
		}
	}

	PendingDelayedOutcomes.Reset();
	const TArray<TSharedPtr<FJsonValue>>* PendArr = nullptr;
	if (Root->TryGetArrayField(TEXT("PendingDelayedOutcomes"), PendArr))
	{
		for (const TSharedPtr<FJsonValue>& V : *PendArr)
		{
			const TSharedPtr<FJsonObject>& O = V->AsObject();
			FSNEDelayedOutcomeEntry Entry;
			Entry.LaterText = FText::FromString(O->GetStringField(TEXT("LaterText")));
			JsonToDelta(O->GetObjectField(TEXT("LaterDelta")), Entry.LaterDelta);
			PendingDelayedOutcomes.Add(Entry);
		}
	}

	CustomerHistories.Reset();
	const TArray<TSharedPtr<FJsonValue>>* HistArr = nullptr;
	if (Root->TryGetArrayField(TEXT("CustomerHistories"), HistArr))
	{
		for (const TSharedPtr<FJsonValue>& V : *HistArr)
		{
			const TSharedPtr<FJsonObject>& O = V->AsObject();
			FSNECustomerHistory H;
			H.VisitCount = O->GetIntegerField(TEXT("VisitCount"));
			H.LastVisitDay = O->GetIntegerField(TEXT("LastVisitDay"));
			H.LastDecision = static_cast<ESNEPreviousDecision>(O->GetIntegerField(TEXT("LastDecision")));
			H.LastIntent = static_cast<ESNECustomerIntent>(O->GetIntegerField(TEXT("LastIntent")));
			const TArray<TSharedPtr<FJsonValue>>* CompletedArr = nullptr;
			if (O->TryGetArrayField(TEXT("CompletedVisitIds"), CompletedArr))
			{
				for (const TSharedPtr<FJsonValue>& C : *CompletedArr)
				{
					H.CompletedVisitIds.Add(FName(*C->AsString()));
				}
			}
			const FName CustomerId(*O->GetStringField(TEXT("CustomerId")));
			CustomerHistories.Add(CustomerId, H);
		}
	}

	ActiveThoughts.Reset();
	const TArray<TSharedPtr<FJsonValue>>* ThoughtArr = nullptr;
	if (Root->TryGetArrayField(TEXT("ActiveThoughts"), ThoughtArr))
	{
		for (const TSharedPtr<FJsonValue>& V : *ThoughtArr)
		{
			const TSharedPtr<FJsonObject>& O = V->AsObject();
			FSNEActiveThought A;
			A.Thought = TSoftObjectPtr<USNEThoughtDataAsset>(FSoftObjectPath(O->GetStringField(TEXT("ThoughtPath"))));
			A.DaysRemaining = O->GetIntegerField(TEXT("DaysRemaining"));
			A.bMatured = O->GetBoolField(TEXT("Matured"));
			ActiveThoughts.Add(A);
		}
	}

	// Active encounter is not saved; loading back to MorningShift/EveningShift mid-encounter
	// would need visit re-selection, so we reset it and let the next StartNextEncounterIfNeeded run.
	ActiveEncounter = FSNEActiveEncounter{};
	ActiveEncounter.bResolved = true;

	RebuildPresentation();
	BroadcastPresentation();
	UE_LOG(LogTemp, Log, TEXT("SNE: Loaded game from '%s'."), *Path);
	return true;
}

bool USNEDialogueGameSubsystem::DoesSaveSlotExist(const FString& SlotName) const
{
	using namespace SNESaveInternal;
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*GetSaveFilePath(SlotName));
}

bool USNEDialogueGameSubsystem::DeleteSaveSlot(const FString& SlotName)
{
	using namespace SNESaveInternal;
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	const FString Path = GetSaveFilePath(SlotName);
	if (!PF.FileExists(*Path))
	{
		return false;
	}
	return PF.DeleteFile(*Path);
}

#undef LOCTEXT_NAMESPACE
