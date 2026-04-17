// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SNEItemDataAsset.generated.h"

UCLASS(BlueprintType, meta = (DisplayName = "SNE Item"))
class SELLNOEVIL_API USNEItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		DisplayName = "Item Id",
		ToolTip = "Stable identifier for logs and analytics. e.g. 'microfilm_scanner'. Other systems reference this item via the Data Asset directly, not this Id."))
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		DisplayName = "Display Name",
		ToolTip = "Name shown to the player in dialogue and UI."))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (
		ToolTip = "Designer-facing description. Not currently shown to the player but can be used later.",
		MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy", meta = (
		DisplayName = "Base Sale Value (MNT)",
		ToolTip = "Cash paid for this item when sold (before tip). No tier fallback.",
		ClampMin = "0"))
	int32 BaseSaleValue = 0;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
