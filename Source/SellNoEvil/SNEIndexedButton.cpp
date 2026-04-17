// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEIndexedButton.h"

void USNEIndexedButton::SetChoiceIndex(const int32 InChoiceIndex)
{
	ChoiceIndex = InChoiceIndex;
}

void USNEIndexedButton::SetChoiceState(const ESNEChoiceType InChoiceType, const bool bInEnabled, const bool bInSelected)
{
	BP_OnChoiceStateChanged(InChoiceType, bInEnabled, bInSelected);
}

void USNEIndexedButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject) && !bInternalClickBound)
	{
		OnClicked.AddDynamic(this, &USNEIndexedButton::HandleInternalClicked);
		bInternalClickBound = true;
	}
}

void USNEIndexedButton::HandleInternalClicked()
{
	OnIndexedClicked.Broadcast(ChoiceIndex);
}
