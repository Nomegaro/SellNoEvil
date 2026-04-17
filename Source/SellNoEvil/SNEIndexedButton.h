// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "SNEDialogueGameSubsystem.h"
#include "SNEIndexedButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSNEIndexedButtonClicked, int32, ChoiceIndex);

UCLASS()
class SELLNOEVIL_API USNEIndexedButton : public UButton
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "SNE|UI")
	FSNEIndexedButtonClicked OnIndexedClicked;

	void SetChoiceIndex(int32 InChoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "SNE|UI")
	void SetChoiceState(ESNEChoiceType InChoiceType, bool bInEnabled, bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "SNE|UI")
	int32 GetChoiceIndex() const { return ChoiceIndex; }

protected:
	// Designer hook: override in a WBP (or C++ subclass) to drive visuals from choice state.
	// Fires after each presentation sync. Default implementation does nothing.
	UFUNCTION(BlueprintImplementableEvent, Category = "SNE|UI", meta = (DisplayName = "On Choice State Changed"))
	void BP_OnChoiceStateChanged(ESNEChoiceType ChoiceType, bool bEnabled, bool bSelected);

	virtual void SynchronizeProperties() override;

private:
	UFUNCTION()
	void HandleInternalClicked();

	UPROPERTY()
	int32 ChoiceIndex = INDEX_NONE;

	bool bInternalClickBound = false;
};
