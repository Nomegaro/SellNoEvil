// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEItemDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "SNEItemDataAsset"

EDataValidationResult USNEItemDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Id.IsNone())
	{
		Context.AddError(LOCTEXT("ItemMissingId", "Item Id is not set. Give it a stable identifier like 'microfilm_scanner'."));
		Result = EDataValidationResult::Invalid;
	}

	if (DisplayName.IsEmpty())
	{
		Context.AddError(LOCTEXT("ItemMissingDisplayName", "Display Name is empty. Players will see a fallback string."));
		Result = EDataValidationResult::Invalid;
	}

	if (BaseSaleValue <= 0)
	{
		Context.AddWarning(LOCTEXT("ItemZeroSaleValue", "Base Sale Value is 0 or negative. Selling this item will earn no money."));
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif
