// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SNEDialogueGameSubsystem.h"

namespace SNETestInternal
{
	static int32 PickProgressChoiceIndex(const FSNEPresentationData& Data)
	{
		auto FindFirstEnabled = [&Data](const ESNEChoiceType Type) -> int32
		{
			for (int32 Index = 0; Index < Data.Choices.Num(); ++Index)
			{
				if (Data.Choices[Index].ChoiceType == Type && Data.Choices[Index].bEnabled)
				{
					return Index;
				}
			}
			return INDEX_NONE;
		};

		const TArray<ESNEChoiceType> Priority = {
			ESNEChoiceType::Sell,
			ESNEChoiceType::NoSell,
			ESNEChoiceType::PrepAction,
			ESNEChoiceType::LunchOption,
			ESNEChoiceType::CleanStoreNow,
			ESNEChoiceType::CleanStoreForTomorrow,
			ESNEChoiceType::SkipCleanupTomorrow,
			ESNEChoiceType::AdvancePhase,
			ESNEChoiceType::RestartDay,
			ESNEChoiceType::Investigate
		};

		for (const ESNEChoiceType Type : Priority)
		{
			const int32 Found = FindFirstEnabled(Type);
			if (Found != INDEX_NONE)
			{
				return Found;
			}
		}

		return INDEX_NONE;
	}

	static bool DriveUntilPhase(USNEDialogueGameSubsystem* Subsystem, const ESNEDayPhase TargetPhase, const int32 MaxSteps = 80)
	{
		if (Subsystem == nullptr)
		{
			return false;
		}

		for (int32 Step = 0; Step < MaxSteps; ++Step)
		{
			const FSNEPresentationData Data = Subsystem->GetCurrentPresentationData();
			if (Data.Phase == TargetPhase)
			{
				return true;
			}

			const int32 ChoiceIndex = PickProgressChoiceIndex(Data);
			if (ChoiceIndex == INDEX_NONE || !Subsystem->ExecuteChoice(ChoiceIndex))
			{
				return false;
			}
		}

		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSNEPhaseOrderTest,
	"SellNoEvil.Dialogue.PhaseOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSNEPhaseOrderTest::RunTest(const FString& Parameters)
{
	USNEDialogueGameSubsystem* Subsystem = NewObject<USNEDialogueGameSubsystem>();
	TestNotNull(TEXT("Subsystem should be created"), Subsystem);
	if (Subsystem == nullptr)
	{
		return false;
	}

	Subsystem->SetRandomSeedForTesting(1337);
	Subsystem->StartDay();

	TArray<ESNEDayPhase> DistinctPhases;
	DistinctPhases.Add(Subsystem->GetCurrentPresentationData().Phase);

	for (int32 Step = 0; Step < 120; ++Step)
	{
		const FSNEPresentationData Data = Subsystem->GetCurrentPresentationData();
		if (Data.Phase == ESNEDayPhase::DayEnd)
		{
			break;
		}

		const int32 ChoiceIndex = SNETestInternal::PickProgressChoiceIndex(Data);
		TestTrue(TEXT("A progress choice should be available"), ChoiceIndex != INDEX_NONE);
		if (ChoiceIndex == INDEX_NONE)
		{
			return false;
		}

		TestTrue(TEXT("Choice execution should succeed"), Subsystem->ExecuteChoice(ChoiceIndex));

		const ESNEDayPhase NewPhase = Subsystem->GetCurrentPresentationData().Phase;
		if (DistinctPhases.Last() != NewPhase)
		{
			DistinctPhases.Add(NewPhase);
		}
	}

	const TArray<ESNEDayPhase> ExpectedOrder = {
		ESNEDayPhase::MorningNews,
		ESNEDayPhase::MorningPrep,
		ESNEDayPhase::MorningShift,
		ESNEDayPhase::Lunch,
		ESNEDayPhase::EveningShift,
		ESNEDayPhase::NightPrep,
		ESNEDayPhase::Closing,
		ESNEDayPhase::RandomEvent,
		ESNEDayPhase::DayEnd
	};

	TestEqual(TEXT("Distinct phase count should match expected"), DistinctPhases.Num(), ExpectedOrder.Num());
	for (int32 Index = 0; Index < ExpectedOrder.Num() && Index < DistinctPhases.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Phase order index %d"), Index), DistinctPhases[Index], ExpectedOrder[Index]);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSNEInvestigateRulesTest,
	"SellNoEvil.Dialogue.InvestigateRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSNEInvestigateRulesTest::RunTest(const FString& Parameters)
{
	USNEDialogueGameSubsystem* Subsystem = NewObject<USNEDialogueGameSubsystem>();
	TestNotNull(TEXT("Subsystem should be created"), Subsystem);
	if (Subsystem == nullptr)
	{
		return false;
	}

	Subsystem->SetRandomSeedForTesting(7);
	Subsystem->StartDay();
	TestTrue(TEXT("Should reach morning shift"), SNETestInternal::DriveUntilPhase(Subsystem, ESNEDayPhase::MorningShift));

	Subsystem->SetEnergyForTesting(0);
	TestFalse(TEXT("Investigate should fail at 0 energy"), Subsystem->TryInvestigate());

	Subsystem->SetEnergyForTesting(5);
	TestTrue(TEXT("Investigate should succeed with enough energy"), Subsystem->TryInvestigate());

	const FSNEPresentationData Data = Subsystem->GetCurrentPresentationData();
	TestTrue(TEXT("Investigate should reveal 2-3 clues"), Data.VisibleClues.Num() >= 2 && Data.VisibleClues.Num() <= 3);

	TSet<FString> UniqueClues;
	for (const FText& Clue : Data.VisibleClues)
	{
		UniqueClues.Add(Clue.ToString());
	}
	TestEqual(TEXT("Clues should not duplicate in one investigation"), UniqueClues.Num(), Data.VisibleClues.Num());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSNEDelayedOutcomeTimingTest,
	"SellNoEvil.Dialogue.DelayedOutcomeTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSNEDelayedOutcomeTimingTest::RunTest(const FString& Parameters)
{
	USNEDialogueGameSubsystem* Subsystem = NewObject<USNEDialogueGameSubsystem>();
	TestNotNull(TEXT("Subsystem should be created"), Subsystem);
	if (Subsystem == nullptr)
	{
		return false;
	}

	Subsystem->SetRandomSeedForTesting(21);
	Subsystem->StartDay();
	TestTrue(TEXT("Should reach morning shift"), SNETestInternal::DriveUntilPhase(Subsystem, ESNEDayPhase::MorningShift));

	const FSNEPresentationData BeforeSell = Subsystem->GetCurrentPresentationData();
	TestEqual(TEXT("Delayed queue starts empty"), Subsystem->GetPendingDelayedOutcomeCountForTesting(), 0);

	TestTrue(TEXT("Resolve sell should succeed"), Subsystem->ResolveSellChoice(true));
	TestEqual(TEXT("Delayed queue should have one outcome after decision"), Subsystem->GetPendingDelayedOutcomeCountForTesting(), 1);

	Subsystem->DebugApplyMorningNewsNow();
	const FSNEPresentationData AfterMorningNews = Subsystem->GetCurrentPresentationData();
	TestEqual(TEXT("Delayed queue should be consumed in morning news"), Subsystem->GetPendingDelayedOutcomeCountForTesting(), 0);
	TestNotEqual(TEXT("Morning news application should change sanity or morality"), BeforeSell.Sanity + BeforeSell.Morality, AfterMorningNews.Sanity + AfterMorningNews.Morality);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSNETipBoundsTest,
	"SellNoEvil.Dialogue.TipBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSNETipBoundsTest::RunTest(const FString& Parameters)
{
	USNEDialogueGameSubsystem* Subsystem = NewObject<USNEDialogueGameSubsystem>();
	TestNotNull(TEXT("Subsystem should be created"), Subsystem);
	if (Subsystem == nullptr)
	{
		return false;
	}

	Subsystem->SetRandomSeedForTesting(42);
	Subsystem->StartDay();
	TestTrue(TEXT("Should reach morning shift"), SNETestInternal::DriveUntilPhase(Subsystem, ESNEDayPhase::MorningShift));

	Subsystem->SetTipChanceForTesting(1.0f);
	const FSNEPresentationData Before = Subsystem->GetCurrentPresentationData();
	const int32 SaleValue = Subsystem->GetActiveSaleValueForTesting();
	TestTrue(TEXT("Active sale value should be positive"), SaleValue > 0);

	TestTrue(TEXT("Sell choice should resolve"), Subsystem->ResolveSellChoice(true));
	const FSNEPresentationData After = Subsystem->GetCurrentPresentationData();

	const int32 MoneyGain = After.Money - Before.Money;
	const int32 MaxExpected = SaleValue + FMath::RoundToInt(static_cast<float>(SaleValue) * 0.05f);
	TestTrue(TEXT("Money gain should include at least sale value"), MoneyGain >= SaleValue);
	TestTrue(TEXT("Money gain should not exceed sale + 5% tip"), MoneyGain <= MaxExpected);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSNEContentIntegrityTest,
	"SellNoEvil.Dialogue.ContentIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSNEContentIntegrityTest::RunTest(const FString& Parameters)
{
	USNEPrototypeContentAsset* Content = USNEPrototypeContentAsset::CreateRuntimeDefaultContent(GetTransientPackage());
	TestNotNull(TEXT("Default content should be created"), Content);
	if (Content == nullptr)
	{
		return false;
	}

	TestTrue(TEXT("Prototype should include at least five customers"), Content->Customers.Num() >= 5);
	for (const FSNECustomerScenario& Customer : Content->Customers)
	{
		TestTrue(FString::Printf(TEXT("%s neutral clues should not be empty"), *Customer.Id.ToString()), Customer.NeutralClues.Num() > 0);
		TestTrue(FString::Printf(TEXT("%s good clues should not be empty"), *Customer.Id.ToString()), Customer.GoodLeaningClues.Num() > 0);
		TestTrue(FString::Printf(TEXT("%s bad clues should not be empty"), *Customer.Id.ToString()), Customer.BadLeaningClues.Num() > 0);
		TestFalse(FString::Printf(TEXT("%s sell good immediate text should not be empty"), *Customer.Id.ToString()), Customer.SellGoodIntentOutcome.ImmediateText.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s sell bad immediate text should not be empty"), *Customer.Id.ToString()), Customer.SellBadIntentOutcome.ImmediateText.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s no-sell good immediate text should not be empty"), *Customer.Id.ToString()), Customer.NoSellGoodIntentOutcome.ImmediateText.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s no-sell bad immediate text should not be empty"), *Customer.Id.ToString()), Customer.NoSellBadIntentOutcome.ImmediateText.IsEmpty());
	}

	return true;
}

#endif
