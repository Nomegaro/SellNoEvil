// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "SNEDialogueGameSubsystem.h"
#include "SNEGameRootWidget.generated.h"

class UButton;
class UBorder;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class USNEDialogueGameSubsystem;
struct FGeometry;
struct FKeyEvent;

UCLASS(BlueprintType)
class SELLNOEVIL_API USNEGameRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SNE|UI")
	void RefreshFromSubsystem();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildLayoutIfNeeded();
	void WireChoiceHandlers();
	int32 FindFirstEnabledChoiceIndex() const;
	USNEDialogueGameSubsystem* GetDialogueSubsystem() const;
	void HandleChoiceClicked(int32 ChoiceIndex);
	void SyncChoiceButtons(const FSNEPresentationData& Data);
	void UpdateTextBlocks(const FSNEPresentationData& Data);

	UFUNCTION()
	void HandleChoice0();

	UFUNCTION()
	void HandleChoice1();

	UFUNCTION()
	void HandleChoice2();

	UFUNCTION()
	void HandleChoice3();

	UFUNCTION()
	void HandleChoice4();

	UFUNCTION()
	void HandleChoice5();

	UFUNCTION()
	void HandleSubsystemPresentationChanged();

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DayPhaseText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MeterText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CustomerText;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> NarrativeScrollBox;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> NarrativeBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActionHeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ChoiceListBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> ChoiceButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ChoiceLabels;

	UPROPERTY(Transient)
	TObjectPtr<USNEDialogueGameSubsystem> CachedSubsystem;
};
