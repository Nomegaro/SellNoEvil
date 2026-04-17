// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "SNEDialogueGameSubsystem.h"
#include "SNEGameRootWidget.generated.h"

class UHorizontalBox;
class UPanelWidget;
class USNEIndexedButton;
class UTextBlock;
class USNEDialogueGameSubsystem;
struct FGeometry;
struct FKeyEvent;

UCLASS(Abstract, BlueprintType)
class SELLNOEVIL_API USNEGameRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SNE|UI")
	void RefreshFromSubsystem();

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// Designer-authored class used to spawn each choice button inside ChoiceListBox.
	// Must derive from USNEIndexedButton. Set this on the WBP in the editor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SNE|UI")
	TSubclassOf<USNEIndexedButton> ChoiceButtonClass;

	// Optional BlueprintImplementableEvent hook for designers who want to react to
	// presentation changes in Blueprint (e.g., play animations) in addition to the
	// default text/choice sync.
	UFUNCTION(BlueprintImplementableEvent, Category = "SNE|UI", meta = (DisplayName = "On Presentation Refreshed"))
	void BP_OnPresentationRefreshed(const FSNEPresentationData& PresentationData);

private:
	void EnsureChoiceWidgets(int32 RequiredCount);
	int32 FindFirstEnabledChoiceIndex() const;
	int32 FindNextEnabledChoiceIndex(int32 StartIndex, int32 Direction) const;
	void MoveChoiceSelection(int32 Direction);
	USNEDialogueGameSubsystem* GetDialogueSubsystem() const;
	bool HandleChoiceClicked(int32 ChoiceIndex);
	void SyncChoiceButtons(const FSNEPresentationData& Data);
	void UpdateTextBlocks(const FSNEPresentationData& Data);

	UFUNCTION()
	void HandleIndexedChoiceClicked(int32 ChoiceIndex);

	UFUNCTION()
	void HandleSubsystemPresentationChanged();

	// UMG-bound widgets. Names must match widgets placed in the designer-authored WBP.
	// OptionalBindWidget means the WBP may omit any of these; C++ no-ops gracefully.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DayPhaseText;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MeterText;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CustomerText;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ClueText;

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionHeaderText;

	// Container that holds the dynamically spawned choice buttons. Must be a UPanelWidget
	// (e.g. HorizontalBox, VerticalBox, WrapBox) in the authored WBP.
	UPROPERTY(Transient, meta = (BindWidget))
	TObjectPtr<UPanelWidget> ChoiceListBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USNEIndexedButton>> ChoiceButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ChoiceLabels;

	UPROPERTY(Transient)
	TObjectPtr<USNEDialogueGameSubsystem> CachedSubsystem;

	FSNEPresentationData LastPresentationData;
	bool bHasPresentationData = false;
	int32 SelectedChoiceIndex = INDEX_NONE;
};
