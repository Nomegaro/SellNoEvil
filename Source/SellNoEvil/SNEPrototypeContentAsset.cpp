// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEPrototypeContentAsset.h"

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
