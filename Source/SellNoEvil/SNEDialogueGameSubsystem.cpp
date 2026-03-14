// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEDialogueGameSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SNEGameRootWidget.h"

#define LOCTEXT_NAMESPACE "SNEDialogueSubsystem"

namespace SNESubsystemInternal
{
	static const TCHAR* DefaultContentAssetPath = TEXT("/Game/Data/DA_SNEPrototypeContent.DA_SNEPrototypeContent");

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

	static FText BuildDeferredEthicsNewsText(
		const FSNECustomerScenario& Scenario,
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
			Scenario.ItemRequested);
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

	DayNumber = 1;
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
	Energy = FMath::Clamp(Energy - RuntimeContentAsset->Defaults.InvestigateEnergyCost, 0, RuntimeContentAsset->Defaults.MaxEnergy);

	TArray<FText> Pool = Scenario.NeutralClues;
	if (ActiveEncounter.Intent == ESNECustomerIntent::Good)
	{
		Pool.Append(Scenario.GoodLeaningClues);
	}
	else
	{
		Pool.Append(Scenario.BadLeaningClues);
	}

	ActiveEncounter.VisibleClues.Reset();
	if (Pool.Num() == 0)
	{
		ActiveEncounter.bInvestigated = true;
		LastEventText = LOCTEXT("InvestigateNoClues", "You investigate, but this customer gives you nothing useful.");
		RebuildPresentation();
		BroadcastPresentation();
		return true;
	}

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
	if (RuntimeContentAsset == nullptr || !RuntimeContentAsset->Customers.IsValidIndex(ActiveEncounter.ScenarioIndex))
	{
		return 0;
	}

	return RuntimeContentAsset->GetSaleValue(RuntimeContentAsset->Customers[ActiveEncounter.ScenarioIndex].PriceTier);
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
		if (WidgetClassToUse == nullptr)
		{
			WidgetClassToUse = USNEGameRootWidget::StaticClass();
		}
		ActiveRootWidget = CreateWidget<UUserWidget>(PlayerController, WidgetClassToUse);
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

	RuntimeContentAsset = LoadObject<USNEPrototypeContentAsset>(nullptr, SNESubsystemInternal::DefaultContentAssetPath);

	if (RuntimeContentAsset == nullptr)
	{
		RuntimeContentAsset = USNEPrototypeContentAsset::CreateRuntimeDefaultContent(this);
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
	PresentationCache.CustomerTitle = Scenario.Nickname;
	PresentationCache.ItemTitle = Scenario.ItemRequested;
	PresentationCache.VisibleClues = ActiveEncounter.VisibleClues;

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

	PresentationCache.BodyText = FText::Format(
		LOCTEXT("ShiftBodyFmt", "Customer {0}/{1}: {2}\nItem Requested: {3}\n\n{4}\n\n{5}"),
		FText::AsNumber(DisplayIndex),
		FText::AsNumber(TargetCount),
		Scenario.Nickname,
		Scenario.ItemRequested,
		Scenario.OpeningDialogue,
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

	ActiveEncounter = FSNEActiveEncounter{};
	ActiveEncounter.ScenarioIndex = ScenarioIndex;
	ActiveEncounter.Intent = RandomStream.FRand() < 0.5f ? ESNECustomerIntent::Good : ESNECustomerIntent::Bad;
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
	const FSNEOutcomeData& Outcome = SelectOutcome(Scenario, bSold, ActiveEncounter.Intent);

	int32 TipAmount = 0;
	if (bSold)
	{
		const int32 SaleValue = GetSaleValue(Scenario);
		Money += SaleValue;

		const float EffectiveTipChance = FMath::Clamp(TipChance * Scenario.TipChanceMultiplier, 0.0f, 1.0f);
		if (RandomStream.FRand() < EffectiveTipChance)
		{
			TipAmount = FMath::RoundToInt(static_cast<float>(SaleValue) * 0.05f);
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

const FSNEOutcomeData& USNEDialogueGameSubsystem::SelectOutcome(const FSNECustomerScenario& Scenario, const bool bSell, const ESNECustomerIntent Intent) const
{
	if (bSell)
	{
		return Intent == ESNECustomerIntent::Good
			? Scenario.SellGoodIntentOutcome
			: Scenario.SellBadIntentOutcome;
	}

	return Intent == ESNECustomerIntent::Good
		? Scenario.NoSellGoodIntentOutcome
		: Scenario.NoSellBadIntentOutcome;
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

int32 USNEDialogueGameSubsystem::GetSaleValue(const FSNECustomerScenario& Scenario) const
{
	return RuntimeContentAsset != nullptr ? RuntimeContentAsset->GetSaleValue(Scenario.PriceTier) : 0;
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
	TArray<int32> SourceIndices;
	for (int32 Index = 0; Index < RuntimeContentAsset->Customers.Num(); ++Index)
	{
		SourceIndices.Add(Index);
	}

	for (int32 Index = 0; Index < SourceIndices.Num(); ++Index)
	{
		const int32 SwapIndex = RandomStream.RandRange(Index, SourceIndices.Num() - 1);
		SourceIndices.Swap(Index, SwapIndex);
	}

	for (int32 Pick = 0; Pick < NeededCount; ++Pick)
	{
		if (Pick < SourceIndices.Num())
		{
			DailyCustomerOrder.Add(SourceIndices[Pick]);
		}
		else
		{
			DailyCustomerOrder.Add(SourceIndices[RandomStream.RandRange(0, SourceIndices.Num() - 1)]);
		}
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

#undef LOCTEXT_NAMESPACE
