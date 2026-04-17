// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEPrototypeContentAsset.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "IAssetTools.h"
#include "Misc/DataValidation.h"
#include "Misc/PackageName.h"
#include "SNEItemDataAsset.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

USNEPrototypeContentAsset* USNEPrototypeContentAsset::CreateRuntimeDefaultContent(UObject* Outer)
{
	// Minimal fallback used only when the authored Data Asset at /Game/Data/DA_SNEPrototypeContent
	// cannot be loaded. All real content (customers, prep actions, lunch options, random events,
	// starting values, etc.) should be authored on that Data Asset in the editor, not here.
	UObject* SafeOuter = Outer != nullptr ? Outer : GetTransientPackage();
	USNEPrototypeContentAsset* Content = NewObject<USNEPrototypeContentAsset>(SafeOuter, USNEPrototypeContentAsset::StaticClass());
	if (Content == nullptr)
	{
		return nullptr;
	}

	Content->Defaults = FSNEPrototypeDefaults{};
	return Content;
}

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "SNEPrototypeContentAsset"

namespace SNEContentValidationInternal
{
	static bool IsOutcomeEmpty(const FSNEOutcomeData& Outcome)
	{
		return Outcome.ImmediateText.IsEmpty();
	}

	static FText FormatPath(const FText& CustomerLabel, const FText& VisitLabel, const FText& FieldLabel)
	{
		return FText::Format(
			LOCTEXT("ValidationPathFmt", "{0} / {1} / {2}"),
			CustomerLabel, VisitLabel, FieldLabel);
	}
}

EDataValidationResult USNEPrototypeContentAsset::IsDataValid(FDataValidationContext& Context) const
{
	using namespace SNEContentValidationInternal;
	EDataValidationResult Result = Super::IsDataValid(Context);

	// --- Defaults sanity ---
	if (Defaults.MorningCustomerCount + Defaults.EveningCustomerCount <= 0)
	{
		Context.AddWarning(LOCTEXT("NoCustomersScheduled", "Defaults: total scheduled customers (Morning + Evening) is 0. No encounters will spawn."));
	}

	if (Defaults.MaxEnergy < Defaults.StartingEnergy)
	{
		Context.AddWarning(LOCTEXT("StartEnergyExceedsMax", "Defaults: Starting Energy is greater than Max Energy. Starting Energy will be clamped."));
	}

	// --- Customers ---
	TSet<FName> SeenCustomerIds;
	for (int32 CIdx = 0; CIdx < Customers.Num(); ++CIdx)
	{
		const FSNECustomerScenario& Customer = Customers[CIdx];
		const FText CustomerLabel = FText::Format(
			LOCTEXT("CustomerLabelFmt", "Customer[{0}] {1}"),
			FText::AsNumber(CIdx),
			Customer.Id.IsNone() ? LOCTEXT("UnnamedCustomer", "(unnamed)") : FText::FromName(Customer.Id));

		if (Customer.Id.IsNone())
		{
			Context.AddError(FText::Format(LOCTEXT("CustomerMissingId", "{0}: Customer Id is not set."), CustomerLabel));
			Result = EDataValidationResult::Invalid;
		}
		else if (SeenCustomerIds.Contains(Customer.Id))
		{
			Context.AddError(FText::Format(
				LOCTEXT("CustomerDuplicateId", "{0}: duplicate Customer Id '{1}'. Each customer must have a unique Id."),
				CustomerLabel, FText::FromName(Customer.Id)));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			SeenCustomerIds.Add(Customer.Id);
		}

		if (Customer.Nickname.IsEmpty())
		{
			Context.AddWarning(FText::Format(LOCTEXT("CustomerMissingNickname", "{0}: Nickname is empty. UI will show the Id instead."), CustomerLabel));
		}

		if (Customer.Visits.Num() == 0)
		{
			Context.AddError(FText::Format(LOCTEXT("CustomerNoVisits", "{0}: has no Visits. This customer will never fire."), CustomerLabel));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		// Visit id registry for this customer (used for Requires/BlockedBy cross-refs).
		TSet<FName> VisitIdsInArc;
		for (const FSNECustomerVisit& V : Customer.Visits)
		{
			if (!V.VisitId.IsNone())
			{
				VisitIdsInArc.Add(V.VisitId);
			}
		}

		TSet<FName> SeenVisitIds;
		for (int32 VIdx = 0; VIdx < Customer.Visits.Num(); ++VIdx)
		{
			const FSNECustomerVisit& Visit = Customer.Visits[VIdx];
			const FText VisitLabel = FText::Format(
				LOCTEXT("VisitLabelFmt", "Visit[{0}] {1}"),
				FText::AsNumber(VIdx),
				Visit.VisitId.IsNone() ? LOCTEXT("UnnamedVisit", "(unnamed)") : FText::FromName(Visit.VisitId));

			if (Visit.VisitId.IsNone())
			{
				Context.AddError(FText::Format(
					LOCTEXT("VisitMissingId", "{0}: Visit Id is not set. History gating on other visits cannot reference it."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldVisitId", "Visit Id"))));
				Result = EDataValidationResult::Invalid;
			}
			else if (SeenVisitIds.Contains(Visit.VisitId))
			{
				Context.AddError(FText::Format(
					LOCTEXT("VisitDuplicateId", "{0}: duplicate Visit Id '{1}' within the same customer."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldVisitId", "Visit Id")),
					FText::FromName(Visit.VisitId)));
				Result = EDataValidationResult::Invalid;
			}
			else
			{
				SeenVisitIds.Add(Visit.VisitId);
			}

			// Pool check.
			if (Visit.RequestedItemPool.Num() == 0)
			{
				Context.AddWarning(FText::Format(
					LOCTEXT("VisitEmptyItemPool", "{0}: Requested Item Pool is empty. The visit will run but no item will be named."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldItemPool", "Requested Item Pool"))));
			}
			else
			{
				for (int32 PIdx = 0; PIdx < Visit.RequestedItemPool.Num(); ++PIdx)
				{
					if (Visit.RequestedItemPool[PIdx].IsNull())
					{
						Context.AddError(FText::Format(
							LOCTEXT("VisitNullItemEntry", "{0}: Requested Item Pool entry [{1}] is empty. Pick an item or remove the row."),
							FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldItemPool", "Requested Item Pool")),
							FText::AsNumber(PIdx)));
						Result = EDataValidationResult::Invalid;
					}
				}
			}

			// Clues: neutral is required, leaning pools are encouraged.
			if (Visit.NeutralClues.Num() == 0)
			{
				Context.AddWarning(FText::Format(
					LOCTEXT("VisitNoNeutralClues", "{0}: no Neutral Clues. Investigate will fall back to a generic 'nothing useful' line."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldNeutralClues", "Neutral Clues"))));
			}
			if (Visit.GoodLeaningClues.Num() == 0)
			{
				Context.AddWarning(FText::Format(
					LOCTEXT("VisitNoGoodClues", "{0}: no Good-Leaning Clues. Good-intent investigations won't feel different from neutral ones."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldGoodClues", "Good-Leaning Clues"))));
			}
			if (Visit.BadLeaningClues.Num() == 0)
			{
				Context.AddWarning(FText::Format(
					LOCTEXT("VisitNoBadClues", "{0}: no Bad-Leaning Clues. Bad-intent investigations won't feel different from neutral ones."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldBadClues", "Bad-Leaning Clues"))));
			}

			// Outcomes: all four required.
			if (IsOutcomeEmpty(Visit.SellGoodIntentOutcome))
			{
				Context.AddError(FText::Format(
					LOCTEXT("VisitOutcomeSellGoodEmpty", "{0}: Sell -> Good Intent outcome has no Immediate Text."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldSellGood", "Sell -> Good Intent"))));
				Result = EDataValidationResult::Invalid;
			}
			if (IsOutcomeEmpty(Visit.SellBadIntentOutcome))
			{
				Context.AddError(FText::Format(
					LOCTEXT("VisitOutcomeSellBadEmpty", "{0}: Sell -> Bad Intent outcome has no Immediate Text."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldSellBad", "Sell -> Bad Intent"))));
				Result = EDataValidationResult::Invalid;
			}
			if (IsOutcomeEmpty(Visit.NoSellGoodIntentOutcome))
			{
				Context.AddError(FText::Format(
					LOCTEXT("VisitOutcomeNoSellGoodEmpty", "{0}: No Sell -> Good Intent outcome has no Immediate Text."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldNoSellGood", "No Sell -> Good Intent"))));
				Result = EDataValidationResult::Invalid;
			}
			if (IsOutcomeEmpty(Visit.NoSellBadIntentOutcome))
			{
				Context.AddError(FText::Format(
					LOCTEXT("VisitOutcomeNoSellBadEmpty", "{0}: No Sell -> Bad Intent outcome has no Immediate Text."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldNoSellBad", "No Sell -> Bad Intent"))));
				Result = EDataValidationResult::Invalid;
			}

			// Opening dialogue is strongly recommended.
			if (Visit.OpeningDialogue.IsEmpty())
			{
				Context.AddWarning(FText::Format(
					LOCTEXT("VisitNoOpeningDialogue", "{0}: Opening Dialogue is empty."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldOpeningDialogue", "Opening Dialogue"))));
			}

			// Condition sanity: cross-reference Requires/BlockedBy against visits in this arc.
			for (const FName& ReqId : Visit.Conditions.RequiresPreviousVisitIds)
			{
				if (ReqId.IsNone()) continue;
				if (!VisitIdsInArc.Contains(ReqId))
				{
					Context.AddError(FText::Format(
						LOCTEXT("VisitReqUnknown", "{0}: Requires Previous Visit '{1}' but no visit in this customer's arc has that Id."),
						FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldRequires", "Requires Previous Visits")),
						FText::FromName(ReqId)));
					Result = EDataValidationResult::Invalid;
				}
				if (ReqId == Visit.VisitId)
				{
					Context.AddError(FText::Format(
						LOCTEXT("VisitReqSelf", "{0}: Requires Previous Visit references itself ('{1}'). This visit can never fire."),
						FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldRequires", "Requires Previous Visits")),
						FText::FromName(ReqId)));
					Result = EDataValidationResult::Invalid;
				}
			}
			for (const FName& BlockId : Visit.Conditions.BlockedByPreviousVisitIds)
			{
				if (BlockId.IsNone()) continue;
				if (!VisitIdsInArc.Contains(BlockId))
				{
					Context.AddWarning(FText::Format(
						LOCTEXT("VisitBlockUnknown", "{0}: Blocked By '{1}' but no visit in this customer's arc has that Id. The block will never trigger."),
						FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldBlocked", "Blocked By Previous Visits")),
						FText::FromName(BlockId)));
				}
			}

			// Range sanity.
			if (Visit.Conditions.MaxVisitCount >= 0 && Visit.Conditions.MaxVisitCount < Visit.Conditions.MinVisitCount)
			{
				Context.AddError(FText::Format(
					LOCTEXT("VisitCountRangeBad", "{0}: Max Visit Count < Min Visit Count. This visit can never fire."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldVisitCount", "Visit Count Range"))));
				Result = EDataValidationResult::Invalid;
			}
			if (Visit.Conditions.MaxDayNumber >= 0 && Visit.Conditions.MaxDayNumber < Visit.Conditions.MinDayNumber)
			{
				Context.AddError(FText::Format(
					LOCTEXT("VisitDayRangeBad", "{0}: Max Day Number < Min Day Number. This visit can never fire."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldDayRange", "Day Range"))));
				Result = EDataValidationResult::Invalid;
			}
			if (Visit.Conditions.MaxMorality < Visit.Conditions.MinMorality)
			{
				Context.AddError(FText::Format(
					LOCTEXT("VisitMoralityRangeBad", "{0}: Max Morality < Min Morality. This visit can never fire."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldMoralityRange", "Morality Range"))));
				Result = EDataValidationResult::Invalid;
			}
			if (Visit.Conditions.MaxSanity < Visit.Conditions.MinSanity)
			{
				Context.AddError(FText::Format(
					LOCTEXT("VisitSanityRangeBad", "{0}: Max Sanity < Min Sanity. This visit can never fire."),
					FormatPath(CustomerLabel, VisitLabel, LOCTEXT("FieldSanityRange", "Sanity Range"))));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	// --- Prep / Lunch / Event id uniqueness ---
	auto CheckIdUniqueness = [&](const auto& Collection, auto GetId, const FText& PoolLabel)
	{
		TSet<FName> Seen;
		for (int32 Idx = 0; Idx < Collection.Num(); ++Idx)
		{
			const FName EntryId = GetId(Collection[Idx]);
			if (EntryId.IsNone())
			{
				Context.AddError(FText::Format(
					LOCTEXT("CollectionMissingId", "{0}[{1}]: Id is not set."),
					PoolLabel, FText::AsNumber(Idx)));
				Result = EDataValidationResult::Invalid;
			}
			else if (Seen.Contains(EntryId))
			{
				Context.AddError(FText::Format(
					LOCTEXT("CollectionDuplicateId", "{0}[{1}]: duplicate Id '{2}'."),
					PoolLabel, FText::AsNumber(Idx), FText::FromName(EntryId)));
				Result = EDataValidationResult::Invalid;
			}
			else
			{
				Seen.Add(EntryId);
			}
		}
	};

	CheckIdUniqueness(MorningPrepActions, [](const FSNEPrepAction& A) { return A.ActionId; }, LOCTEXT("PoolMorningPrep", "Morning Prep Actions"));
	CheckIdUniqueness(NightPrepActions,   [](const FSNEPrepAction& A) { return A.ActionId; }, LOCTEXT("PoolNightPrep", "Night Prep Actions"));
	CheckIdUniqueness(LunchOptions,       [](const FSNELunchOption& L) { return L.OptionId; }, LOCTEXT("PoolLunch", "Lunch Options"));
	CheckIdUniqueness(RandomEvents,       [](const FSNERandomEvent& E) { return E.EventId; }, LOCTEXT("PoolEvents", "Random Events"));

	return Result;
}

// ---------- Sample customer generator ----------

namespace SNESampleContentInternal
{
	static const TCHAR* ItemsPackageRoot = TEXT("/Game/Items");

	struct FSampleItemSpec
	{
		const TCHAR* AssetName; // e.g. "DA_Item_PhotoRibbon"
		const TCHAR* Id;        // stable logical id
		FText DisplayName;
		FText Description;
		int32 BaseSaleValue;
	};

	static USNEItemDataAsset* FindOrCreateItemAsset(const FSampleItemSpec& Spec)
	{
		const FString AssetName = Spec.AssetName;
		const FString PackagePath = FString::Printf(TEXT("%s/%s"), ItemsPackageRoot, *AssetName);
		const FString ObjectPath = PackagePath + TEXT(".") + AssetName;

		// Fast path: already exists.
		if (USNEItemDataAsset* Existing = LoadObject<USNEItemDataAsset>(nullptr, *ObjectPath))
		{
			return Existing;
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		UPackage* Package = CreatePackage(*PackagePath);
		if (Package == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("SNE: Failed to create package '%s'."), *PackagePath);
			return nullptr;
		}
		Package->FullyLoad();

		USNEItemDataAsset* Item = NewObject<USNEItemDataAsset>(Package, USNEItemDataAsset::StaticClass(), *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		if (Item == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("SNE: NewObject failed for item '%s'."), *AssetName);
			return nullptr;
		}

		Item->Id = FName(Spec.Id);
		Item->DisplayName = Spec.DisplayName;
		Item->Description = Spec.Description;
		Item->BaseSaleValue = Spec.BaseSaleValue;

		FAssetRegistryModule::AssetCreated(Item);
		Item->MarkPackageDirty();

		// Save to disk so the soft pointer from the Customers array resolves on next load.
		const FString FileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		UPackage::SavePackage(Package, Item, *FileName, SaveArgs);

		return Item;
	}

	static FSNEOutcomeData MakeOutcome(const FText& ImmediateText, int32 Money, int32 Energy, int32 Sanity, int32 Morality,
	                                   const FText& LaterText = FText::GetEmpty(), int32 LaterMoney = 0, int32 LaterSanity = 0, int32 LaterMorality = 0)
	{
		FSNEOutcomeData Out;
		Out.ImmediateText = ImmediateText;
		Out.ImmediateDelta.Money = Money;
		Out.ImmediateDelta.Energy = Energy;
		Out.ImmediateDelta.Sanity = Sanity;
		Out.ImmediateDelta.Morality = Morality;
		Out.LaterText = LaterText;
		Out.LaterDelta.Money = LaterMoney;
		Out.LaterDelta.Sanity = LaterSanity;
		Out.LaterDelta.Morality = LaterMorality;
		return Out;
	}
}

void USNEPrototypeContentAsset::GenerateSampleCustomers()
{
	using namespace SNESampleContentInternal;

	// 1) Make sure all required items exist as sibling assets.
	const TArray<FSampleItemSpec> ItemSpecs = {
		{ TEXT("DA_Item_MicrofilmScanner"), TEXT("microfilm_scanner"),
		  LOCTEXT("Item_MicrofilmScanner", "Microfilm Scanner + Photo Ribbon"),
		  LOCTEXT("Desc_MicrofilmScanner", "Old-world optical scanner. Cheap, quiet, reads faded archives."), 80000 },
		{ TEXT("DA_Item_Ampoules"), TEXT("restricted_ampoules"),
		  LOCTEXT("Item_Ampoules", "Restricted Analgesic Ampoules"),
		  LOCTEXT("Desc_Ampoules", "Sealed field-surgery analgesics. Legal only for licensed clinics."), 170000 },
		{ TEXT("DA_Item_DemolitionCord"), TEXT("demolition_cord"),
		  LOCTEXT("Item_DemolitionCord", "Demolition Cord + Trigger Caps"),
		  LOCTEXT("Desc_DemolitionCord", "Mining-grade blast cord with non-fragmenting caps."), 320000 },
		{ TEXT("DA_Item_RelayChips"), TEXT("relay_chips"),
		  LOCTEXT("Item_RelayChips", "Relay Chips + Narrowband Amplifier"),
		  LOCTEXT("Desc_RelayChips", "Hobbyist radio parts. Civilian band only."), 80000 },
		{ TEXT("DA_Item_SatHandset"), TEXT("sat_handset"),
		  LOCTEXT("Item_SatHandset", "Encrypted Satellite Handset + Spare Battery"),
		  LOCTEXT("Desc_SatHandset", "Serial-tracked sat-comms device. Registered sale required."), 320000 },
	};

	TMap<FString, USNEItemDataAsset*> Items;
	for (const FSampleItemSpec& Spec : ItemSpecs)
	{
		if (USNEItemDataAsset* Asset = FindOrCreateItemAsset(Spec))
		{
			Items.Add(FString(Spec.AssetName), Asset);
		}
	}

	auto ItemPtr = [&Items](const TCHAR* AssetName)
	{
		if (USNEItemDataAsset** Found = Items.Find(FString(AssetName)))
		{
			return TSoftObjectPtr<USNEItemDataAsset>(*Found);
		}
		return TSoftObjectPtr<USNEItemDataAsset>();
	};

	// 2) Build sample customer arcs (single visit each, VisitId = "first_meeting").
	auto MakeCustomer = [](const TCHAR* Id, const FText& Nickname, int32 Age, float TipMult) -> FSNECustomerScenario
	{
		FSNECustomerScenario C;
		C.Id = FName(Id);
		C.Nickname = Nickname;
		C.Age = Age;
		C.TipChanceMultiplier = TipMult;
		C.bAllowMultipleVisitsPerDay = false;
		C.ArcEndBehavior = ESNEArcEndBehavior::LoopLast;
		return C;
	};

	auto MakeFirstVisit = [](float GoodIntent, const FText& Opening) -> FSNECustomerVisit
	{
		FSNECustomerVisit V;
		V.VisitId = TEXT("first_meeting");
		V.Priority = 0;
		V.GoodIntentChance = GoodIntent;
		V.OpeningDialogue = Opening;
		return V;
	};

	Customers.Reset();

	// --- Customer 1: Quiet Student ---
	{
		FSNECustomerScenario C = MakeCustomer(TEXT("quiet_student"), LOCTEXT("S1_Nick", "Architecture Student"), 21, 0.95f);
		FSNECustomerVisit V = MakeFirstVisit(0.60f, LOCTEXT("S1_Open",
			"\"I need to scan old district plans tonight. If I miss this chance, people get trapped.\""));
		V.RequestedItemPool.Add(ItemPtr(TEXT("DA_Item_MicrofilmScanner")));
		V.NeutralClues = {
			LOCTEXT("S1_N1", "He folds and unfolds a transit map until the paper edges turn white."),
			LOCTEXT("S1_N2", "His backpack is stitched by hand with thread in three different colors."),
			LOCTEXT("S1_N3", "Ink on his fingers, archive dust on his sleeves.")
		};
		V.GoodLeaningClues = {
			LOCTEXT("S1_G1", "The map marks hospital corridors and shelter basements, not checkpoints."),
			LOCTEXT("S1_G2", "He asks if the scanner can preserve handwritten names in faded margins.")
		};
		V.BadLeaningClues = {
			LOCTEXT("S1_B1", "A second folded note lists patrol rotations, not classroom locations."),
			LOCTEXT("S1_B2", "He asks whether your sales logs are ever audited in real time.")
		};
		V.SellGoodIntentOutcome   = MakeOutcome(LOCTEXT("S1_SG", "He bows. \"Thank you. Maybe those missing names can be found.\""), 0, 0, -1, 2,
			LOCTEXT("S1_SG_L", "Morning News: a missing-family list goes online and helps many people."), -12000, 1, 1);
		V.SellBadIntentOutcome    = MakeOutcome(LOCTEXT("S1_SB", "He looks relieved and leaves before the receipt prints."), 0, 0, -2, -2,
			LOCTEXT("S1_SB_L", "Bulletin: fake travel papers trap refugees. Inspectors request seller records."), -25000, -1, -2);
		V.NoSellGoodIntentOutcome = MakeOutcome(LOCTEXT("S1_NG", "He nods slowly. \"I understand.\" He folds the map and leaves."), 0, 0, -1, -2,
			LOCTEXT("S1_NG_L", "Records lost in a blackout. Families lose another way to trace loved ones."), 0, -1, -1);
		V.NoSellBadIntentOutcome  = MakeOutcome(LOCTEXT("S1_NB", "\"Then I'll try another shop,\" he says."), 0, 0, 0, 1,
			LOCTEXT("S1_NB_L", "A trafficking group fails after it cannot copy permit data. Your stall is not named."), 10000, 1, 1);
		C.Visits.Add(V);
		Customers.Add(C);
	}

	// --- Customer 2: Worried Nurse ---
	{
		FSNECustomerScenario C = MakeCustomer(TEXT("worried_nurse"), LOCTEXT("S2_Nick", "Camp Paramedic"), 33, 1.05f);
		FSNECustomerVisit V = MakeFirstVisit(0.62f, LOCTEXT("S2_Open",
			"\"We have three surgeries, one generator, and no supply delivery. I just need stock.\""));
		V.RequestedItemPool.Add(ItemPtr(TEXT("DA_Item_Ampoules")));
		V.NeutralClues = {
			LOCTEXT("S2_N1", "Her badge is real, but the laminate has been resealed twice."),
			LOCTEXT("S2_N2", "She keeps glancing at the market clock like she is racing curfew."),
			LOCTEXT("S2_N3", "Blood specks stain one cuff under a fresh wash.")
		};
		V.GoodLeaningClues = {
			LOCTEXT("S2_G1", "She recites pediatric and adult doses from memory, then asks you to verify anyway."),
			LOCTEXT("S2_G2", "\"I can pay tax. I cannot pay in lives.\"")
		};
		V.BadLeaningClues = {
			LOCTEXT("S2_B1", "Her requisition stamp belongs to a clinic that closed last winter."),
			LOCTEXT("S2_B2", "She asks which ampoules are easiest to dilute without clouding.")
		};
		V.SellGoodIntentOutcome   = MakeOutcome(LOCTEXT("S2_SG", "\"You gave us a real chance tonight.\""), 0, 0, -1, 2,
			LOCTEXT("S2_SG_L", "An emergency ward treats many patients with no deaths. Batch numbers requested."), -8000, 1, 1);
		V.SellBadIntentOutcome    = MakeOutcome(LOCTEXT("S2_SB", "She smiles once the box is in her bag and skips the receipt."), 0, 0, -2, -2,
			LOCTEXT("S2_SB_L", "Fake pain kits cause overdoses. Enforcement teams copy vendor logs."), -18000, -1, -2);
		V.NoSellGoodIntentOutcome = MakeOutcome(LOCTEXT("S2_NG", "She closes her eyes. \"Understood. I will decide who gets the last dose.\""), 0, 0, -1, -2,
			LOCTEXT("S2_NG_L", "Emergency tents report preventable deaths after supplies run out."), 0, -1, -1);
		V.NoSellBadIntentOutcome  = MakeOutcome(LOCTEXT("S2_NB", "\"Fine. I'll buy from someone else.\""), 0, 0, 0, 1,
			LOCTEXT("S2_NB_L", "Customs stop a drug ring using fake clinic seals. Compliant vendors get a bonus."), 15000, 1, 1);
		C.Visits.Add(V);
		Customers.Add(C);
	}

	// --- Customer 3: Ex Miner ---
	{
		FSNECustomerScenario C = MakeCustomer(TEXT("ex_miner"), LOCTEXT("S3_Nick", "Tunnel Foreman"), 46, 0.75f);
		FSNECustomerVisit V = MakeFirstVisit(0.48f, LOCTEXT("S3_Open",
			"\"If we don't clear the collapse before dawn, a thousand people stay trapped.\""));
		V.RequestedItemPool.Add(ItemPtr(TEXT("DA_Item_DemolitionCord")));
		V.NeutralClues = {
			LOCTEXT("S3_N1", "His coat still carries a decommissioned state-mining patch."),
			LOCTEXT("S3_N2", "He sets each word down like measured powder."),
			LOCTEXT("S3_N3", "Both wrists are scarred from old blasting cable burns.")
		};
		V.GoodLeaningClues = {
			LOCTEXT("S3_G1", "He asks for non-fragmenting caps to reduce civilian injury risk."),
			LOCTEXT("S3_G2", "His permit lists a blocked metro shelter route, not industrial property.")
		};
		V.BadLeaningClues = {
			LOCTEXT("S3_B1", "He asks which mix creates the loudest shock wave in enclosed corridors."),
			LOCTEXT("S3_B2", "The permit references a warehouse district abandoned years ago.")
		};
		V.SellGoodIntentOutcome   = MakeOutcome(LOCTEXT("S3_SG", "He signs every form and adds: \"For civilian rescue only.\""), 0, 0, -1, 2,
			LOCTEXT("S3_SG_L", "A collapsed transit route is cleared, opening a legal evacuation path."), 25000, 1, 1);
		V.SellBadIntentOutcome    = MakeOutcome(LOCTEXT("S3_SB", "He pockets the triggers before you finish counting."), 0, 0, -2, -2,
			LOCTEXT("S3_SB_L", "An aid depot is sabotaged. Inspectors sweep vendors."), -30000, -1, -2);
		V.NoSellGoodIntentOutcome = MakeOutcome(LOCTEXT("S3_NG", "He nods. \"Then we pray the concrete moves on its own.\""), 0, 0, -1, -2,
			LOCTEXT("S3_NG_L", "Rescue crews fail to reopen the shelter tunnel."), 0, -1, -1);
		V.NoSellBadIntentOutcome  = MakeOutcome(LOCTEXT("S3_NB", "He stares at the camera dome, then leaves without bargaining."), 0, 0, 0, 1,
			LOCTEXT("S3_NB_L", "A planned demolition attack is stopped when the group fails to get proper triggers."), 12000, 1, 2);
		C.Visits.Add(V);
		Customers.Add(C);
	}

	// --- Customer 4: Street Tinkerer ---
	{
		FSNECustomerScenario C = MakeCustomer(TEXT("street_tinkerer"), LOCTEXT("S4_Nick", "Signal Mechanic"), 26, 1.2f);
		FSNECustomerVisit V = MakeFirstVisit(0.58f, LOCTEXT("S4_Open",
			"\"Whole neighborhoods have no signal. I can rebuild a network if I get parts before curfew.\""));
		V.RequestedItemPool.Add(ItemPtr(TEXT("DA_Item_RelayChips")));
		V.NeutralClues = {
			LOCTEXT("S4_N1", "He organizes screws by length and alloy in reused medicine caps."),
			LOCTEXT("S4_N2", "A solder burn on his thumb is less than a day old."),
			LOCTEXT("S4_N3", "His schematics are drawn on ration-card backs.")
		};
		V.GoodLeaningClues = {
			LOCTEXT("S4_G1", "He maps relay points at clinics, schools, and aid kitchens."),
			LOCTEXT("S4_G2", "He refuses military frequencies and asks only for civilian bands.")
		};
		V.BadLeaningClues = {
			LOCTEXT("S4_B1", "He asks if the amplifier can spoof state emergency channels."),
			LOCTEXT("S4_B2", "His contact list is numeric codenames, not names.")
		};
		V.SellGoodIntentOutcome   = MakeOutcome(LOCTEXT("S4_SG", "\"If this works, people can call doctors in time.\""), 0, 0, -1, 1,
			LOCTEXT("S4_SG_L", "A volunteer mesh keeps clinics online during a blackout."), 18000, 1, 1);
		V.SellBadIntentOutcome    = MakeOutcome(LOCTEXT("S4_SB", "He pays extra to skip the inventory stamp and leaves quickly."), 0, 0, -2, -2,
			LOCTEXT("S4_SB_L", "Fake evacuation signals push refugees into seizure checkpoints."), -22000, -1, -2);
		V.NoSellGoodIntentOutcome = MakeOutcome(LOCTEXT("S4_NG", "He nods. \"Then the dead zones stay dead.\""), 0, 0, -1, -2,
			LOCTEXT("S4_NG_L", "Emergency requests from outer districts fail to reach dispatch during the storm."), 0, -1, -1);
		V.NoSellBadIntentOutcome  = MakeOutcome(LOCTEXT("S4_NB", "\"Fine. Someone else will sell it.\""), 0, 0, 0, 1,
			LOCTEXT("S4_NB_L", "A fake-network operation is shut down. Licensed vendors receive a small rebate."), 9000, 1, 1);
		C.Visits.Add(V);
		Customers.Add(C);
	}

	// --- Customer 5: Night Courier ---
	{
		FSNECustomerScenario C = MakeCustomer(TEXT("night_courier"), LOCTEXT("S5_Nick", "Transit Mother"), 35, 0.7f);
		FSNECustomerVisit V = MakeFirstVisit(0.45f, LOCTEXT("S5_Open",
			"\"My daughter is already offshore. If I miss tonight's convoy, she grows up without me.\""));
		V.RequestedItemPool.Add(ItemPtr(TEXT("DA_Item_SatHandset")));
		V.NeutralClues = {
			LOCTEXT("S5_N1", "She keeps touching a plastic wristband with a child's name half-rubbed off."),
			LOCTEXT("S5_N2", "Her passport sleeve is empty but carefully preserved."),
			LOCTEXT("S5_N3", "She scans every reflective surface before speaking.")
		};
		V.GoodLeaningClues = {
			LOCTEXT("S5_G1", "One saved voice message: a child asking if she is coming soon."),
			LOCTEXT("S5_G2", "She accepts tracking terms and asks you to log the serial cleanly.")
		};
		V.BadLeaningClues = {
			LOCTEXT("S5_B1", "She asks how long a sat handset can run while masking convoy beacons."),
			LOCTEXT("S5_B2", "Her route card lists known smuggling chokepoints, not legal docks.")
		};
		V.SellGoodIntentOutcome   = MakeOutcome(LOCTEXT("S5_SG", "\"Wait for me. I'm coming.\""), 0, 0, -1, 2,
			LOCTEXT("S5_SG_L", "A refugee convoy reroutes around a raid after a satellite warning."), 22000, 1, 2);
		V.SellBadIntentOutcome    = MakeOutcome(LOCTEXT("S5_SB", "Cash only, no signature. She leaves before the camera sweep returns."), 0, 0, -2, -2,
			LOCTEXT("S5_SB_L", "Armed runners use untraceable sat-comms to lead refugees into capture routes."), -28000, -1, -2);
		V.NoSellGoodIntentOutcome = MakeOutcome(LOCTEXT("S5_NG", "She nods once, then deletes a message she never sends."), 0, 0, -1, -2,
			LOCTEXT("S5_NG_L", "Legal convoy misses departure after a comms failure. Family reunions delayed."), 0, -1, -1);
		V.NoSellBadIntentOutcome  = MakeOutcome(LOCTEXT("S5_NB", "\"Then I go in blind,\" she says."), 0, 0, 0, 1,
			LOCTEXT("S5_NB_L", "A hidden sat-comms cache is seized during a failed handoff."), 14000, 1, 2);
		C.Visits.Add(V);
		Customers.Add(C);
	}

	MarkPackageDirty();
	UE_LOG(LogTemp, Log, TEXT("SNE: Generated %d sample customers."), Customers.Num());
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
