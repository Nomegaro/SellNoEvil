// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEPrototypeContentAsset.h"

#define LOCTEXT_NAMESPACE "SNEPrototypeContent"

namespace SNEContentInternal
{
	static FSNEOutcomeData MakeOutcome(
		const FText& ImmediateText,
		const FSNEMeterDelta& ImmediateDelta,
		const FText& LaterText,
		const FSNEMeterDelta& LaterDelta)
	{
		FSNEOutcomeData Outcome;
		Outcome.ImmediateText = ImmediateText;
		Outcome.ImmediateDelta = ImmediateDelta;
		Outcome.LaterText = LaterText;
		Outcome.LaterDelta = LaterDelta;
		return Outcome;
	}

	static FSNEMeterDelta MakeDelta(const int32 Money, const int32 Energy, const int32 Sanity, const int32 Morality, const float TipChance = 0.0f)
	{
		FSNEMeterDelta Delta;
		Delta.Money = Money;
		Delta.Energy = Energy;
		Delta.Sanity = Sanity;
		Delta.Morality = Morality;
		Delta.TipChance = TipChance;
		return Delta;
	}

	static FSNECustomerScenario BuildQuietStudent()
	{
		FSNECustomerScenario Customer;
		Customer.Id = TEXT("quiet_student");
		Customer.Nickname = LOCTEXT("QuietStudentNick", "Architecture Student");
		Customer.Age = 21;
		Customer.ItemRequested = LOCTEXT("QuietStudentItem", "Microfilm Scanner + Photo Ribbon");
		Customer.PriceTier = ESNEPriceTier::Cheap;
		Customer.TipChanceMultiplier = 0.95f;
		Customer.OpeningDialogue = LOCTEXT("QuietStudentDialog", "\"I need to scan old district plans tonight. If I miss this chance, people get trapped.\"");

		Customer.NeutralClues = {
			LOCTEXT("QSNeutral1", "He folds and unfolds a transit map until the paper edges turn white."),
			LOCTEXT("QSNeutral2", "His backpack is stitched by hand with thread in three different colors."),
			LOCTEXT("QSNeutral3", "He keeps one eye on the inspection mirror above your stall."),
			LOCTEXT("QSNeutral4", "His accent shifts halfway through one sentence, like he learned to mask it."),
			LOCTEXT("QSNeutral5", "He has ink on his fingers and dust from old archives on his sleeves.")
		};
		Customer.GoodLeaningClues = {
			LOCTEXT("QSGood1", "The map marks hospital corridors and shelter basements, not checkpoints."),
			LOCTEXT("QSGood2", "He asks if the scanner can preserve handwritten names in faded margins."),
			LOCTEXT("QSGood3", "When you mention refugees, he says, \"No one should disappear between districts.\""),
			LOCTEXT("QSGood4", "He insists he only needs enough ribbon for one night.")
		};
		Customer.BadLeaningClues = {
			LOCTEXT("QSBad1", "A second folded note lists patrol rotations, not classroom locations."),
			LOCTEXT("QSBad2", "He asks whether your sales logs are ever audited in real time."),
			LOCTEXT("QSBad3", "He calls people \"units\" when describing movement through tunnels."),
			LOCTEXT("QSBad4", "His route sketch circles your stall as if it were one checkpoint among many.")
		};

		Customer.SellGoodIntentOutcome = MakeOutcome(
			LOCTEXT("QSSellGoodImmediate", "He bows. \"Thank you. Maybe those missing names can be found.\""),
			MakeDelta(0, 0, -1, 2),
			LOCTEXT("QSSellGoodLater", "Morning news: a missing-family list goes online and helps many people. Inspectors also flag your sale."),
			MakeDelta(-12000, 0, 1, 1));

		Customer.SellBadIntentOutcome = MakeOutcome(
			LOCTEXT("QSSellBadImmediate", "He looks relieved and leaves before the receipt prints."),
			MakeDelta(0, 0, -2, -2),
			LOCTEXT("QSSellBadLater", "State news: fake travel papers are used to trap refugees. Officials request seller records from Nar-n-Tol."),
			MakeDelta(-25000, 0, -1, -2));

		Customer.NoSellGoodIntentOutcome = MakeOutcome(
			LOCTEXT("QSNoSellGoodImmediate", "He nods slowly. \"I understand.\" He folds the map and leaves."),
			MakeDelta(0, 0, -1, -2),
			LOCTEXT("QSNoSellGoodLater", "News: old records are lost in a power outage. Families lose another way to trace loved ones."),
			MakeDelta(0, 0, -1, -1));

		Customer.NoSellBadIntentOutcome = MakeOutcome(
			LOCTEXT("QSNoSellBadImmediate", "\"Then I'll try another shop,\" he says."),
			MakeDelta(0, 0, 0, 1),
			LOCTEXT("QSNoSellBadLater", "Inspection notice: a trafficking group fails after it cannot copy permit data. Your stall is not named."),
			MakeDelta(10000, 0, 1, 1));

		return Customer;
	}

	static FSNECustomerScenario BuildWorriedNurse()
	{
		FSNECustomerScenario Customer;
		Customer.Id = TEXT("worried_nurse");
		Customer.Nickname = LOCTEXT("WorriedNurseNick", "Camp Paramedic");
		Customer.Age = 33;
		Customer.ItemRequested = LOCTEXT("WorriedNurseItem", "Restricted Analgesic Ampoules");
		Customer.PriceTier = ESNEPriceTier::Moderate;
		Customer.TipChanceMultiplier = 1.05f;
		Customer.OpeningDialogue = LOCTEXT("WorriedNurseDialog", "\"We have three surgeries, one generator, and no supply delivery. I just need stock.\"");

		Customer.NeutralClues = {
			LOCTEXT("WNNeutral1", "Her badge is real, but the laminate has been resealed twice."),
			LOCTEXT("WNNeutral2", "She keeps glancing at the market clock like she is racing transport curfew."),
			LOCTEXT("WNNeutral3", "Blood specks stain one cuff under a fresh wash.")
		};
		Customer.GoodLeaningClues = {
			LOCTEXT("WNGood1", "She recites pediatric and adult doses from memory, then asks you to verify anyway."),
			LOCTEXT("WNGood2", "She requests sealed lots and insists on proper cold-chain packaging."),
			LOCTEXT("WNGood3", "When you mention bribes, she says, \"I can pay tax. I cannot pay in lives.\"")
		};
		Customer.BadLeaningClues = {
			LOCTEXT("WNBad1", "Her requisition stamp belongs to a clinic that closed last winter."),
			LOCTEXT("WNBad2", "She asks which ampoules are easiest to dilute without clouding."),
			LOCTEXT("WNBad3", "A text on her phone reads: \"Party gets loud at midnight. Bring enough.\"")
		};

		Customer.SellGoodIntentOutcome = MakeOutcome(
			LOCTEXT("WNSellGoodImmediate", "She takes the case and says, \"You gave us a real chance tonight.\""),
			MakeDelta(0, 0, -1, 2),
			LOCTEXT("WNSellGoodLater", "Regional news: an emergency ward treats many patients with no deaths. An inspector asks for your batch numbers."),
			MakeDelta(-8000, 0, 1, 1));
		Customer.SellBadIntentOutcome = MakeOutcome(
			LOCTEXT("WNSellBadImmediate", "She smiles once the box is in her bag and skips the receipt."),
			MakeDelta(0, 0, -2, -2),
			LOCTEXT("WNSellBadLater", "Morning bulletin: fake pain kits cause overdoses in Nar-n-Tol clubs. Enforcement teams copy vendor logs."),
			MakeDelta(-18000, 0, -1, -2));
		Customer.NoSellGoodIntentOutcome = MakeOutcome(
			LOCTEXT("WNNoSellGoodImmediate", "She closes her eyes. \"Understood. I will decide who gets the last dose.\""),
			MakeDelta(0, 0, -1, -2),
			LOCTEXT("WNNoSellGoodLater", "News wire: emergency tents report preventable deaths after supplies run out. No clinic is named, but everyone knows."),
			MakeDelta(0, 0, -1, -1));
		Customer.NoSellBadIntentOutcome = MakeOutcome(
			LOCTEXT("WNNoSellBadImmediate", "She says, \"Fine. I'll buy from someone else.\""),
			MakeDelta(0, 0, 0, 1),
			LOCTEXT("WNNoSellBadLater", "Customs stop a drug ring using fake clinic seals. Compliant vendors get a small bonus."),
			MakeDelta(15000, 0, 1, 1));

		return Customer;
	}

	static FSNECustomerScenario BuildExMiner()
	{
		FSNECustomerScenario Customer;
		Customer.Id = TEXT("ex_miner");
		Customer.Nickname = LOCTEXT("ExMinerNick", "Tunnel Foreman");
		Customer.Age = 46;
		Customer.ItemRequested = LOCTEXT("ExMinerItem", "Demolition Cord + Trigger Caps");
		Customer.PriceTier = ESNEPriceTier::Expensive;
		Customer.TipChanceMultiplier = 0.75f;
		Customer.OpeningDialogue = LOCTEXT("ExMinerDialog", "\"If we don't clear the collapse before dawn, a thousand people stay trapped.\"");

		Customer.NeutralClues = {
			LOCTEXT("EMNeutral1", "His coat still carries a decommissioned state-mining patch."),
			LOCTEXT("EMNeutral2", "He sets each word down like measured powder."),
			LOCTEXT("EMNeutral3", "Both wrists are scarred from old blasting cable burns.")
		};
		Customer.GoodLeaningClues = {
			LOCTEXT("EMGood1", "He asks for non-fragmenting caps to reduce civilian injury risk."),
			LOCTEXT("EMGood2", "His permit lists a blocked metro shelter route, not industrial property."),
			LOCTEXT("EMGood3", "He requests your worst, oldest stock so fresh supplies remain for hospitals.")
		};
		Customer.BadLeaningClues = {
			LOCTEXT("EMBad1", "He asks which mix creates the loudest shock wave in enclosed corridors."),
			LOCTEXT("EMBad2", "The permit references a warehouse district abandoned years ago."),
			LOCTEXT("EMBad3", "He flinches when you mention evacuation timing and blast radius.")
		};

		Customer.SellGoodIntentOutcome = MakeOutcome(
			LOCTEXT("EMSellGoodImmediate", "He signs every form and adds: \"For civilian rescue only.\""),
			MakeDelta(0, 0, -1, 2),
			LOCTEXT("EMSellGoodLater", "Morning report: a collapsed transit route is cleared, opening a legal evacuation path for two districts."),
			MakeDelta(25000, 0, 1, 1));
		Customer.SellBadIntentOutcome = MakeOutcome(
			LOCTEXT("EMSellBadImmediate", "He pockets the triggers before you finish counting and leaves through a side path."),
			MakeDelta(0, 0, -2, -2),
			LOCTEXT("EMSellBadLater", "Dawn sirens: an aid depot is sabotaged. Supply convoys reroute for days, and inspectors sweep vendors."),
			MakeDelta(-30000, 0, -1, -2));
		Customer.NoSellGoodIntentOutcome = MakeOutcome(
			LOCTEXT("EMNoSellGoodImmediate", "He nods. \"Then we pray the concrete moves on its own.\""),
			MakeDelta(0, 0, -1, -2),
			LOCTEXT("EMNoSellGoodLater", "Morning update: rescue crews fail to reopen the shelter tunnel; overnight cold deaths rise."),
			MakeDelta(0, 0, -1, -1));
		Customer.NoSellBadIntentOutcome = MakeOutcome(
			LOCTEXT("EMNoSellBadImmediate", "He stares at the camera dome, then leaves without bargaining."),
			MakeDelta(0, 0, 0, 1),
			LOCTEXT("EMNoSellBadLater", "Security bulletin: a planned demolition attack is stopped after the group fails to get proper triggers."),
			MakeDelta(12000, 0, 1, 2));

		return Customer;
	}

	static FSNECustomerScenario BuildStreetTinkerer()
	{
		FSNECustomerScenario Customer;
		Customer.Id = TEXT("street_tinkerer");
		Customer.Nickname = LOCTEXT("StreetTinkererNick", "Signal Mechanic");
		Customer.Age = 26;
		Customer.ItemRequested = LOCTEXT("StreetTinkererItem", "Relay Chips + Narrowband Amplifier");
		Customer.PriceTier = ESNEPriceTier::Cheap;
		Customer.TipChanceMultiplier = 1.2f;
		Customer.OpeningDialogue = LOCTEXT("StreetTinkererDialog", "\"Whole neighborhoods have no signal. I can rebuild a network if I get parts before curfew.\"");

		Customer.NeutralClues = {
			LOCTEXT("STNeutral1", "He organizes screws by length and alloy in reused medicine caps."),
			LOCTEXT("STNeutral2", "A solder burn on his thumb is less than a day old."),
			LOCTEXT("STNeutral3", "His schematics are drawn on ration-card backs.")
		};
		Customer.GoodLeaningClues = {
			LOCTEXT("STGood1", "He maps relay points at clinics, schools, and aid kitchens."),
			LOCTEXT("STGood2", "He asks for surge-safe parts so emergency calls do not drop mid-storm."),
			LOCTEXT("STGood3", "He refuses military frequencies and asks only for civilian bands.")
		};
		Customer.BadLeaningClues = {
			LOCTEXT("STBad1", "He asks if the amplifier can spoof state emergency channels."),
			LOCTEXT("STBad2", "His contact list is numeric codenames, not names."),
			LOCTEXT("STBad3", "He calls false alerts \"traffic shaping\" with a straight face.")
		};

		Customer.SellGoodIntentOutcome = MakeOutcome(
			LOCTEXT("STSellGoodImmediate", "\"If this works,\" he says, \"people can call doctors in time.\""),
			MakeDelta(0, 0, -1, 1, 0.01f),
			LOCTEXT("STSellGoodLater", "City feed: a volunteer mesh keeps clinics online during a blackout. Families use it to find missing relatives."),
			MakeDelta(18000, 0, 1, 1));
		Customer.SellBadIntentOutcome = MakeOutcome(
			LOCTEXT("STSellBadImmediate", "He pays extra to skip the inventory stamp and leaves quickly."),
			MakeDelta(0, 0, -2, -2),
			LOCTEXT("STSellBadLater", "Morning panic: fake evacuation signals push refugees into seizure checkpoints. Authorities trace hardware lots to Nar-n-Tol sellers."),
			MakeDelta(-22000, 0, -1, -2));
		Customer.NoSellGoodIntentOutcome = MakeOutcome(
			LOCTEXT("STNoSellGoodImmediate", "He nods and says, \"Then the dead zones stay dead.\""),
			MakeDelta(0, 0, -1, -2),
			LOCTEXT("STNoSellGoodLater", "Communications report: emergency requests from outer districts fail to reach dispatch during the storm."),
			MakeDelta(0, 0, -1, -1));
		Customer.NoSellBadIntentOutcome = MakeOutcome(
			LOCTEXT("STNoSellBadImmediate", "\"Fine,\" he says. \"Someone else will sell it.\""),
			MakeDelta(0, 0, 0, 1),
			LOCTEXT("STNoSellBadLater", "Fraud unit shuts down a fake-network operation. Licensed vendors receive a small legal rebate."),
			MakeDelta(9000, 0, 1, 1));

		return Customer;
	}

	static FSNECustomerScenario BuildNightCourier()
	{
		FSNECustomerScenario Customer;
		Customer.Id = TEXT("night_courier");
		Customer.Nickname = LOCTEXT("NightCourierNick", "Transit Mother");
		Customer.Age = 35;
		Customer.ItemRequested = LOCTEXT("NightCourierItem", "Encrypted Satellite Handset + Spare Battery");
		Customer.PriceTier = ESNEPriceTier::Expensive;
		Customer.TipChanceMultiplier = 0.7f;
		Customer.OpeningDialogue = LOCTEXT("NightCourierDialog", "\"My daughter is already offshore. If I miss tonight's convoy, she grows up without me.\"");

		Customer.NeutralClues = {
			LOCTEXT("NCNeutral1", "She keeps touching a plastic wristband with a child's name half-rubbed off."),
			LOCTEXT("NCNeutral2", "Her passport sleeve is empty but carefully preserved."),
			LOCTEXT("NCNeutral3", "She scans every reflective surface before speaking.")
		};
		Customer.GoodLeaningClues = {
			LOCTEXT("NCGood1", "She has one saved voice message: a child asking if she is coming soon."),
			LOCTEXT("NCGood2", "She asks whether the handset can transmit identity proof to legal transit brokers."),
			LOCTEXT("NCGood3", "She accepts tracking terms and asks you to log the serial cleanly.")
		};
		Customer.BadLeaningClues = {
			LOCTEXT("NCBad1", "She asks how long a sat handset can run while masking convoy beacons."),
			LOCTEXT("NCBad2", "Her route card lists known smuggling chokepoints, not legal docks."),
			LOCTEXT("NCBad3", "She says, \"No names,\" before you ask for any.")
		};

		Customer.SellGoodIntentOutcome = MakeOutcome(
			LOCTEXT("NCSellGoodImmediate", "She records a short message before leaving: \"Wait for me. I'm coming.\""),
			MakeDelta(0, 0, -1, 2),
			LOCTEXT("NCSellGoodLater", "Morning dispatch: a refugee convoy reroutes around a raid after a satellite warning, saving several families."),
			MakeDelta(22000, 0, 1, 2));
		Customer.SellBadIntentOutcome = MakeOutcome(
			LOCTEXT("NCSellBadImmediate", "Cash only, no signature. She leaves before the camera sweep returns."),
			MakeDelta(0, 0, -2, -2),
			LOCTEXT("NCSellBadLater", "Border report: armed runners use untraceable sat-comms to lead refugees into capture routes."),
			MakeDelta(-28000, 0, -1, -2));
		Customer.NoSellGoodIntentOutcome = MakeOutcome(
			LOCTEXT("NCNoSellGoodImmediate", "She nods once, then deletes a message she never sends."),
			MakeDelta(0, 0, -1, -2),
			LOCTEXT("NCNoSellGoodLater", "Transit bulletin: the legal convoy misses departure after a comms failure. Family reunions are delayed."),
			MakeDelta(0, 0, -1, -1));
		Customer.NoSellBadIntentOutcome = MakeOutcome(
			LOCTEXT("NCNoSellBadImmediate", "\"Then I go in blind,\" she says."),
			MakeDelta(0, 0, 0, 1),
			LOCTEXT("NCNoSellBadLater", "Customs notice: a hidden sat-comms cache is seized during a failed handoff. Vendors linked to that route are quietly rewarded."),
			MakeDelta(14000, 0, 1, 2));

		return Customer;
	}
}

USNEPrototypeContentAsset* USNEPrototypeContentAsset::CreateRuntimeDefaultContent(UObject* Outer)
{
	UObject* SafeOuter = Outer != nullptr ? Outer : GetTransientPackage();
	USNEPrototypeContentAsset* Content = NewObject<USNEPrototypeContentAsset>(SafeOuter, USNEPrototypeContentAsset::StaticClass());
	if (Content == nullptr)
	{
		return nullptr;
	}

	Content->Defaults = FSNEPrototypeDefaults{};

	Content->Customers = {
		SNEContentInternal::BuildQuietStudent(),
		SNEContentInternal::BuildWorriedNurse(),
		SNEContentInternal::BuildExMiner(),
		SNEContentInternal::BuildStreetTinkerer(),
		SNEContentInternal::BuildNightCourier()
	};

	{
		FSNEPrepAction Action;
		Action.ActionId = TEXT("morning_hot_tea");
		Action.Label = LOCTEXT("MorningTeaLabel", "Brew Bitter Tea");
		Action.Description = LOCTEXT("MorningTeaDesc", "You read inspection headlines while the kettle boils. A short reset before opening.");
		Action.MoneyCost = 5000;
		Action.ResultDelta = SNEContentInternal::MakeDelta(0, 1, 1, 0);
		Content->MorningPrepActions.Add(Action);
	}
	{
		FSNEPrepAction Action;
		Action.ActionId = TEXT("morning_window_sign");
		Action.Label = LOCTEXT("MorningSignLabel", "Hang a 'Licensed & Compliant' Sign");
		Action.Description = LOCTEXT("MorningSignDesc", "This tells inspectors you follow rules, and warns risky buyers.");
		Action.ResultDelta = SNEContentInternal::MakeDelta(0, 0, 0, 0, 0.02f);
		Content->MorningPrepActions.Add(Action);
	}
	{
		FSNEPrepAction Action;
		Action.ActionId = TEXT("morning_social_call");
		Action.Label = LOCTEXT("MorningSocialLabel", "Call Neighbor Stall Owners");
		Action.Description = LOCTEXT("MorningSocialDesc", "You trade rumors about patrol routes, blacklists, and who vanished overnight.");
		Action.ResultDelta = SNEContentInternal::MakeDelta(0, 0, 1, 1);
		Content->MorningPrepActions.Add(Action);
	}

	{
		FSNEPrepAction Action;
		Action.ActionId = TEXT("night_reflect");
		Action.Label = LOCTEXT("NightReflectLabel", "Write the Unsent Journal");
		Action.Description = LOCTEXT("NightReflectDesc", "You write thoughts you'll never send. It still helps.");
		Action.ResultDelta = SNEContentInternal::MakeDelta(0, 0, 1, 1);
		Content->NightPrepActions.Add(Action);
	}
	{
		FSNEPrepAction Action;
		Action.ActionId = TEXT("night_stock_count");
		Action.Label = LOCTEXT("NightStockLabel", "Audit Inventory and Logs");
		Action.Description = LOCTEXT("NightStockDesc", "You match inventory to receipts before inspectors do.");
		Action.ResultDelta = SNEContentInternal::MakeDelta(20000, -1, 0, 0);
		Content->NightPrepActions.Add(Action);
	}
	{
		FSNEPrepAction Action;
		Action.ActionId = TEXT("night_clean_counter");
		Action.Label = LOCTEXT("NightCleanLabel", "Erase Fingerprints, Polish Counter");
		Action.Description = LOCTEXT("NightCleanDesc", "You clean the counter and remove traces.");
		Action.ResultDelta = SNEContentInternal::MakeDelta(0, 0, 0, 1, 0.01f);
		Content->NightPrepActions.Add(Action);
	}

	{
		FSNELunchOption Option;
		Option.OptionId = TEXT("lunch_quick_snack");
		Option.Label = LOCTEXT("LunchSnackLabel", "Quick Noodles at the Stall");
		Option.Description = LOCTEXT("LunchSnackDesc", "Cheap and fast. You eat while reading seizure notices.");
		Option.MoneyCost = 3000;
		Option.ResultDelta = SNEContentInternal::MakeDelta(0, 1, 0, 0);
		Content->LunchOptions.Add(Option);
	}
	{
		FSNELunchOption Option;
		Option.OptionId = TEXT("lunch_nutritious");
		Option.Label = LOCTEXT("LunchNutriLabel", "Warm Meal in the Back Alley Cafe");
		Option.Description = LOCTEXT("LunchNutriDesc", "Costs more, but you can sit in a spot with fewer cameras.");
		Option.MoneyCost = 12000;
		Option.ResultDelta = SNEContentInternal::MakeDelta(0, 2, 1, 0);
		Content->LunchOptions.Add(Option);
	}
	{
		FSNELunchOption Option;
		Option.OptionId = TEXT("lunch_family_pack");
		Option.Label = LOCTEXT("LunchFamilyLabel", "Packed Lunch from Home");
		Option.Description = LOCTEXT("LunchFamilyDesc", "Simple food from home. A reminder of who you are trying to protect.");
		Option.ResultDelta = SNEContentInternal::MakeDelta(0, 2, 1, 1);
		Content->LunchOptions.Add(Option);
	}

	{
		FSNERandomEvent Event;
		Event.EventId = TEXT("event_calm_radio");
		Event.EventText = LOCTEXT("EventCalmRadio", "State radio reads names of missing people who were found alive.");
		Event.ResultDelta = SNEContentInternal::MakeDelta(0, 0, 1, 0);
		Content->RandomEvents.Add(Event);
	}
	{
		FSNERandomEvent Event;
		Event.EventId = TEXT("event_tax_notice");
		Event.EventText = LOCTEXT("EventTaxNotice", "An inspection courier slides a new tax bill under your shutter: 'FINAL WARNING'.");
		Event.ResultDelta = SNEContentInternal::MakeDelta(-15000, 0, -1, 0);
		Content->RandomEvents.Add(Event);
	}
	{
		FSNERandomEvent Event;
		Event.EventId = TEXT("event_regular_postcard");
		Event.EventText = LOCTEXT("EventRegularPostcard", "A former customer leaves a postcard: \"I made it through. I remember your help.\"");
		Event.ResultDelta = SNEContentInternal::MakeDelta(0, 0, 1, 1);
		Content->RandomEvents.Add(Event);
	}
	{
		FSNERandomEvent Event;
		Event.EventId = TEXT("event_short_blackout");
		Event.EventText = LOCTEXT("EventShortBlackout", "A blackout kills cameras for eleven minutes. When power returns, three stalls are looted and one owner is missing.");
		Event.ResultDelta = SNEContentInternal::MakeDelta(0, -1, -1, 0);
		Content->RandomEvents.Add(Event);
	}

	return Content;
}

int32 USNEPrototypeContentAsset::GetSaleValue(const ESNEPriceTier PriceTier) const
{
	switch (PriceTier)
	{
	case ESNEPriceTier::Cheap:
		return Defaults.CheapSaleValue;
	case ESNEPriceTier::Moderate:
		return Defaults.ModerateSaleValue;
	case ESNEPriceTier::Expensive:
		return Defaults.ExpensiveSaleValue;
	default:
		break;
	}

	return Defaults.CheapSaleValue;
}

#undef LOCTEXT_NAMESPACE
