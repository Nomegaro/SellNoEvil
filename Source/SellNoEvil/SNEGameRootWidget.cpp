// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEGameRootWidget.h"

#include "SNEIndexedButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ButtonSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "SNEGameRootWidget"

void USNEGameRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetKeyboardFocus();

	USNEDialogueGameSubsystem* Subsystem = GetDialogueSubsystem();
	if (CachedSubsystem != Subsystem)
	{
		if (CachedSubsystem != nullptr)
		{
			CachedSubsystem->OnPresentationChanged.RemoveDynamic(this, &USNEGameRootWidget::HandleSubsystemPresentationChanged);
		}
		CachedSubsystem = Subsystem;
		if (CachedSubsystem != nullptr)
		{
			CachedSubsystem->OnPresentationChanged.AddDynamic(this, &USNEGameRootWidget::HandleSubsystemPresentationChanged);
		}
	}

	RefreshFromSubsystem();
}

FReply USNEGameRootWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();
	const FKey VirtualAcceptKey = EKeys::Virtual_Gamepad_Accept.GetVirtualKey();
	int32 ChoiceIndex = INDEX_NONE;
	bool bDidProcessChoiceInput = false;

	if (PressedKey == EKeys::Left || PressedKey == EKeys::Up)
	{
		MoveChoiceSelection(-1);
		return FReply::Handled();
	}
	if (PressedKey == EKeys::Right || PressedKey == EKeys::Down)
	{
		MoveChoiceSelection(1);
		return FReply::Handled();
	}
	if (PressedKey == EKeys::Tab)
	{
		MoveChoiceSelection(InKeyEvent.IsShiftDown() ? -1 : 1);
		return FReply::Handled();
	}

	if (PressedKey == EKeys::One || PressedKey == EKeys::NumPadOne)
	{
		ChoiceIndex = 0;
		bDidProcessChoiceInput = true;
	}
	else if (PressedKey == EKeys::Two || PressedKey == EKeys::NumPadTwo)
	{
		ChoiceIndex = 1;
		bDidProcessChoiceInput = true;
	}
	else if (PressedKey == EKeys::Three || PressedKey == EKeys::NumPadThree)
	{
		ChoiceIndex = 2;
		bDidProcessChoiceInput = true;
	}
	else if (PressedKey == EKeys::Four || PressedKey == EKeys::NumPadFour)
	{
		ChoiceIndex = 3;
		bDidProcessChoiceInput = true;
	}
	else if (PressedKey == EKeys::Five || PressedKey == EKeys::NumPadFive)
	{
		ChoiceIndex = 4;
		bDidProcessChoiceInput = true;
	}
	else if (PressedKey == EKeys::Six || PressedKey == EKeys::NumPadSix)
	{
		ChoiceIndex = 5;
		bDidProcessChoiceInput = true;
	}
	else if (PressedKey == EKeys::Seven || PressedKey == EKeys::NumPadSeven)
	{
		ChoiceIndex = 6;
		bDidProcessChoiceInput = true;
	}
	else if (PressedKey == EKeys::Eight || PressedKey == EKeys::NumPadEight)
	{
		ChoiceIndex = 7;
		bDidProcessChoiceInput = true;
	}
	else if (PressedKey == EKeys::Nine || PressedKey == EKeys::NumPadNine)
	{
		ChoiceIndex = 8;
		bDidProcessChoiceInput = true;
	}
	else if (PressedKey == EKeys::Zero || PressedKey == EKeys::NumPadZero)
	{
		ChoiceIndex = 9;
		bDidProcessChoiceInput = true;
	}
	else if (PressedKey == EKeys::Enter || PressedKey == VirtualAcceptKey || PressedKey == EKeys::SpaceBar)
	{
		ChoiceIndex = SelectedChoiceIndex;
		bDidProcessChoiceInput = true;
	}

	if (ChoiceIndex != INDEX_NONE
		&& bHasPresentationData
		&& LastPresentationData.Choices.IsValidIndex(ChoiceIndex)
		&& LastPresentationData.Choices[ChoiceIndex].bEnabled)
	{
		SelectedChoiceIndex = ChoiceIndex;
		SyncChoiceButtons(LastPresentationData);
		HandleChoiceClicked(ChoiceIndex);
		return FReply::Handled();
	}

	if (bDidProcessChoiceInput)
	{
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USNEGameRootWidget::RefreshFromSubsystem()
{
	USNEDialogueGameSubsystem* Subsystem = GetDialogueSubsystem();
	if (Subsystem == nullptr)
	{
		return;
	}

	LastPresentationData = Subsystem->GetCurrentPresentationData();
	bHasPresentationData = true;
	UpdateTextBlocks(LastPresentationData);
	SyncChoiceButtons(LastPresentationData);
	BP_OnPresentationRefreshed(LastPresentationData);
}

void USNEGameRootWidget::EnsureChoiceWidgets(const int32 RequiredCount)
{
	if (WidgetTree == nullptr || ChoiceListBox == nullptr)
	{
		return;
	}

	TSubclassOf<USNEIndexedButton> ButtonClass = ChoiceButtonClass;
	if (ButtonClass == nullptr)
	{
		ButtonClass = USNEIndexedButton::StaticClass();
	}

	while (ChoiceButtons.Num() < RequiredCount)
	{
		const int32 Index = ChoiceButtons.Num();
		USNEIndexedButton* ChoiceButton = WidgetTree->ConstructWidget<USNEIndexedButton>(ButtonClass, *FString::Printf(TEXT("ChoiceButton%d"), Index));
		if (ChoiceButton == nullptr)
		{
			break;
		}
		ChoiceButton->SetChoiceIndex(Index);
		ChoiceButton->OnIndexedClicked.AddDynamic(this, &USNEGameRootWidget::HandleIndexedChoiceClicked);

		UTextBlock* ChoiceLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ChoiceLabel%d"), Index));
		ChoiceLabel->SetAutoWrapText(true);
		ChoiceLabel->SetJustification(ETextJustify::Center);
		ChoiceButton->AddChild(ChoiceLabel);

		ChoiceListBox->AddChild(ChoiceButton);

		ChoiceButtons.Add(ChoiceButton);
		ChoiceLabels.Add(ChoiceLabel);
	}
}

int32 USNEGameRootWidget::FindFirstEnabledChoiceIndex() const
{
	const int32 MaxButtons = FMath::Min(ChoiceButtons.Num(), ChoiceLabels.Num());
	for (int32 Index = 0; Index < MaxButtons; ++Index)
	{
		if (ChoiceButtons[Index] != nullptr
			&& ChoiceButtons[Index]->GetVisibility() == ESlateVisibility::Visible
			&& ChoiceButtons[Index]->GetIsEnabled())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 USNEGameRootWidget::FindNextEnabledChoiceIndex(const int32 StartIndex, const int32 Direction) const
{
	if (!bHasPresentationData || LastPresentationData.Choices.Num() == 0)
	{
		return INDEX_NONE;
	}

	const int32 ChoiceCount = LastPresentationData.Choices.Num();
	const int32 StepDirection = Direction >= 0 ? 1 : -1;
	int32 CursorIndex = StartIndex;

	for (int32 Step = 0; Step < ChoiceCount; ++Step)
	{
		CursorIndex += StepDirection;
		if (CursorIndex < 0)
		{
			CursorIndex = ChoiceCount - 1;
		}
		else if (CursorIndex >= ChoiceCount)
		{
			CursorIndex = 0;
		}

		if (LastPresentationData.Choices.IsValidIndex(CursorIndex) && LastPresentationData.Choices[CursorIndex].bEnabled)
		{
			return CursorIndex;
		}
	}

	return INDEX_NONE;
}

void USNEGameRootWidget::MoveChoiceSelection(const int32 Direction)
{
	if (!bHasPresentationData || LastPresentationData.Choices.Num() == 0)
	{
		return;
	}

	int32 NewSelection = INDEX_NONE;
	if (SelectedChoiceIndex == INDEX_NONE
		|| !LastPresentationData.Choices.IsValidIndex(SelectedChoiceIndex)
		|| !LastPresentationData.Choices[SelectedChoiceIndex].bEnabled)
	{
		NewSelection = FindFirstEnabledChoiceIndex();
	}
	else
	{
		NewSelection = FindNextEnabledChoiceIndex(SelectedChoiceIndex, Direction);
	}

	if (NewSelection == INDEX_NONE)
	{
		return;
	}

	SelectedChoiceIndex = NewSelection;
	SyncChoiceButtons(LastPresentationData);
}

USNEDialogueGameSubsystem* USNEGameRootWidget::GetDialogueSubsystem() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI != nullptr ? GI->GetSubsystem<USNEDialogueGameSubsystem>() : nullptr;
}

bool USNEGameRootWidget::HandleChoiceClicked(const int32 ChoiceIndex)
{
	if (USNEDialogueGameSubsystem* Subsystem = GetDialogueSubsystem())
	{
		return Subsystem->ExecuteChoice(ChoiceIndex);
	}

	return false;
}

void USNEGameRootWidget::HandleIndexedChoiceClicked(const int32 ChoiceIndex)
{
	SelectedChoiceIndex = ChoiceIndex;
	if (bHasPresentationData)
	{
		SyncChoiceButtons(LastPresentationData);
	}
	HandleChoiceClicked(ChoiceIndex);
}

void USNEGameRootWidget::SyncChoiceButtons(const FSNEPresentationData& Data)
{
	EnsureChoiceWidgets(Data.Choices.Num());

	const int32 MaxButtons = FMath::Min(ChoiceButtons.Num(), ChoiceLabels.Num());
	int32 VisibleChoices = 0;
	int32 EnabledChoices = 0;

	for (int32 Index = 0; Index < MaxButtons; ++Index)
	{
		const bool bVisible = Data.Choices.IsValidIndex(Index);
		ChoiceButtons[Index]->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bVisible)
		{
			continue;
		}
		++VisibleChoices;

		const FSNEChoiceData& Choice = Data.Choices[Index];
		const bool bSelected = Index == SelectedChoiceIndex;
		ChoiceButtons[Index]->SetChoiceIndex(Index);
		ChoiceButtons[Index]->SetIsEnabled(Choice.bEnabled);
		ChoiceButtons[Index]->SetChoiceState(Choice.ChoiceType, Choice.bEnabled, bSelected);
		ChoiceLabels[Index]->SetText(FText::Format(
			LOCTEXT("ChoiceHotkeyFmt", "[{0}] {1}"),
			FText::AsNumber(Index + 1),
			Choice.Label));
		if (Choice.bEnabled)
		{
			++EnabledChoices;
		}
	}

	if (!Data.Choices.IsValidIndex(SelectedChoiceIndex)
		|| !Data.Choices[SelectedChoiceIndex].bEnabled)
	{
		SelectedChoiceIndex = FindFirstEnabledChoiceIndex();
	}

	if (ActionHeaderText != nullptr)
	{
		ActionHeaderText->SetText(FText::Format(
			LOCTEXT("ActionHeaderFmt", "Choices ({0}/{1})"),
			FText::AsNumber(EnabledChoices),
			FText::AsNumber(VisibleChoices)));
	}
}

void USNEGameRootWidget::UpdateTextBlocks(const FSNEPresentationData& Data)
{
	if (HeaderText != nullptr)
	{
		HeaderText->SetText(Data.HeaderText.IsEmpty()
			? LOCTEXT("GameNameTitle", "SELL NO EVIL")
			: Data.HeaderText);
	}

	if (DayPhaseText != nullptr)
	{
		const UEnum* PhaseEnum = StaticEnum<ESNEDayPhase>();
		const FText PhaseName = PhaseEnum != nullptr
			? PhaseEnum->GetDisplayNameTextByValue(static_cast<int64>(Data.Phase))
			: FText::FromString(TEXT("Unknown"));
		DayPhaseText->SetText(FText::Format(
			LOCTEXT("DayPhaseFmt", "Day {0} | {1}"),
			FText::AsNumber(Data.DayNumber),
			PhaseName));
	}

	const USNEDialogueGameSubsystem* Subsystem = GetDialogueSubsystem();
	int32 MaxEnergy = 8;
	if (Subsystem != nullptr)
	{
		if (const USNEPrototypeContentAsset* Content = Subsystem->GetResolvedContent())
		{
			MaxEnergy = FMath::Max(1, Content->Defaults.MaxEnergy);
		}
	}

	if (MeterText != nullptr)
	{
		MeterText->SetText(FText::Format(
			LOCTEXT("CompactMeterFmt", "Cash {0} MNT | E {1}/{2} | Sanity {3} | Morality {4} | Tip {5}%"),
			FText::AsNumber(Data.Money),
			FText::AsNumber(Data.Energy),
			FText::AsNumber(MaxEnergy),
			FText::AsNumber(Data.Sanity),
			FText::AsNumber(Data.Morality),
			FText::AsNumber(FMath::RoundToInt(Data.TipChance * 100.0f))));
	}

	if (CustomerText != nullptr)
	{
		if (!Data.CustomerTitle.IsEmpty() || !Data.ItemTitle.IsEmpty())
		{
			CustomerText->SetVisibility(ESlateVisibility::Visible);
			CustomerText->SetText(FText::Format(
				LOCTEXT("CustomerInfoFmt", "Visitor: {0} | Item: {1}"),
				Data.CustomerTitle.IsEmpty() ? LOCTEXT("UnknownCustomerText", "Unidentified") : Data.CustomerTitle,
				Data.ItemTitle.IsEmpty() ? LOCTEXT("UnknownItemText", "Item not declared") : Data.ItemTitle));
		}
		else
		{
			CustomerText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (BodyText != nullptr)
	{
		BodyText->SetText(Data.BodyText.IsEmpty() ? LOCTEXT("FallbackBodyVisible", "...") : Data.BodyText);
	}

	if (ClueText != nullptr)
	{
		if (Data.VisibleClues.Num() == 0)
		{
			ClueText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			FString ClueBlock = TEXT("Notes:\n");
			for (const FText& Clue : Data.VisibleClues)
			{
				ClueBlock += FString::Printf(TEXT("  - %s\n"), *Clue.ToString());
			}
			ClueText->SetText(FText::FromString(ClueBlock));
			ClueText->SetVisibility(ESlateVisibility::Visible);
		}
	}
}

void USNEGameRootWidget::HandleSubsystemPresentationChanged()
{
	RefreshFromSubsystem();
}

#undef LOCTEXT_NAMESPACE
