// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEGameRootWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "SNEGameRootWidget"

namespace SNEWidgetInternal
{
	static FLinearColor GetChoiceColor(const ESNEChoiceType Type, const bool bEnabled)
	{
		if (!bEnabled)
		{
			return FLinearColor(0.16f, 0.15f, 0.14f, 0.92f);
		}

		switch (Type)
		{
		case ESNEChoiceType::Sell:
			return FLinearColor(0.48f, 0.29f, 0.13f, 1.0f);
		case ESNEChoiceType::NoSell:
			return FLinearColor(0.19f, 0.31f, 0.40f, 1.0f);
		case ESNEChoiceType::Investigate:
			return FLinearColor(0.33f, 0.30f, 0.17f, 1.0f);
		case ESNEChoiceType::AdvancePhase:
		case ESNEChoiceType::RestartDay:
			return FLinearColor(0.32f, 0.30f, 0.27f, 1.0f);
		default:
			return FLinearColor(0.25f, 0.24f, 0.22f, 1.0f);
		}
	}
}

TSharedRef<SWidget> USNEGameRootWidget::RebuildWidget()
{
	BuildLayoutIfNeeded();
	return Super::RebuildWidget();
}

void USNEGameRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTemp, Log, TEXT("SNE: Root widget NativeConstruct"));
	SetIsFocusable(true);
	SetKeyboardFocus();

	WireChoiceHandlers();

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
	int32 ChoiceIndex = INDEX_NONE;

	if (PressedKey == EKeys::One || PressedKey == EKeys::NumPadOne)
	{
		ChoiceIndex = 0;
	}
	else if (PressedKey == EKeys::Two || PressedKey == EKeys::NumPadTwo)
	{
		ChoiceIndex = 1;
	}
	else if (PressedKey == EKeys::Three || PressedKey == EKeys::NumPadThree)
	{
		ChoiceIndex = 2;
	}
	else if (PressedKey == EKeys::Four || PressedKey == EKeys::NumPadFour)
	{
		ChoiceIndex = 3;
	}
	else if (PressedKey == EKeys::Five || PressedKey == EKeys::NumPadFive)
	{
		ChoiceIndex = 4;
	}
	else if (PressedKey == EKeys::Six || PressedKey == EKeys::NumPadSix)
	{
		ChoiceIndex = 5;
	}
	else if (PressedKey == EKeys::Enter || PressedKey == EKeys::Virtual_Accept || PressedKey == EKeys::SpaceBar)
	{
		ChoiceIndex = FindFirstEnabledChoiceIndex();
	}

	if (ChoiceIndex != INDEX_NONE)
	{
		HandleChoiceClicked(ChoiceIndex);
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USNEGameRootWidget::RefreshFromSubsystem()
{
	USNEDialogueGameSubsystem* Subsystem = GetDialogueSubsystem();
	if (Subsystem == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: Root widget refresh skipped, subsystem is null"));
		return;
	}

	const FSNEPresentationData Data = Subsystem->GetCurrentPresentationData();
	UE_LOG(LogTemp, Verbose, TEXT("SNE: Root widget refreshed. Phase=%d, Choices=%d"), static_cast<int32>(Data.Phase), Data.Choices.Num());
	UpdateTextBlocks(Data);
	SyncChoiceButtons(Data);
}

void USNEGameRootWidget::BuildLayoutIfNeeded()
{
	if (WidgetTree == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: BuildLayoutIfNeeded failed, WidgetTree is null"));
		return;
	}

	if (RootBox != nullptr
		&& RootBorder != nullptr
		&& HeaderText != nullptr
		&& BodyText != nullptr
		&& NarrativeScrollBox != nullptr
		&& ChoiceListBox != nullptr)
	{
		UE_LOG(LogTemp, Verbose, TEXT("SNE: BuildLayoutIfNeeded skipped, layout already built"));
		return;
	}

	if (WidgetTree->RootWidget != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("SNE: Replacing unexpected pre-existing root widget: %s"), *WidgetTree->RootWidget->GetClass()->GetName());
		WidgetTree->RootWidget = nullptr;
	}

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RootBorder"));
	RootBorder->SetPadding(FMargin(12.0f));
	RootBorder->SetBrushColor(FLinearColor(0.07f, 0.07f, 0.07f, 0.93f));
	UCanvasPanel* CanvasRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CanvasRoot"));
	WidgetTree->RootWidget = CanvasRoot;
	if (UCanvasPanelSlot* CanvasSlot = CanvasRoot->AddChildToCanvas(RootBorder))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CanvasSlot->SetOffsets(FMargin(16.0f, 14.0f, 16.0f, 14.0f));
		CanvasSlot->SetAutoSize(false);
	}

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootVBox"));
	RootBorder->SetContent(RootBox);

	HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeaderText"));
	HeaderText->SetText(LOCTEXT("DefaultHeader", "SELL NO EVIL"));
	HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.88f, 0.74f, 1.0f)));
	if (UVerticalBoxSlot* VBoxSlot = RootBox->AddChildToVerticalBox(HeaderText))
	{
		VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	DayPhaseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DayPhaseText"));
	DayPhaseText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.86f, 0.80f, 1.0f)));
	if (UVerticalBoxSlot* VBoxSlot = RootBox->AddChildToVerticalBox(DayPhaseText))
	{
		VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	UBorder* MeterPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MeterPanel"));
	MeterPanel->SetBrushColor(FLinearColor(0.16f, 0.14f, 0.12f, 0.96f));
	MeterPanel->SetPadding(FMargin(6.0f));
	if (UVerticalBoxSlot* VBoxSlot = RootBox->AddChildToVerticalBox(MeterPanel))
	{
		VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	MeterText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MeterText"));
	MeterText->SetAutoWrapText(false);
	MeterText->SetColorAndOpacity(FSlateColor(FLinearColor(0.67f, 0.70f, 0.68f, 1.0f)));
	MeterPanel->SetContent(MeterText);

	CustomerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CustomerText"));
	CustomerText->SetAutoWrapText(true);
	CustomerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.84f, 0.70f, 1.0f)));
	CustomerText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* VBoxSlot = RootBox->AddChildToVerticalBox(CustomerText))
	{
		VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	UBorder* NarrativePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NarrativePanel"));
	NarrativePanel->SetBrushColor(FLinearColor(0.14f, 0.13f, 0.12f, 0.97f));
	NarrativePanel->SetPadding(FMargin(10.0f));
	if (UVerticalBoxSlot* VBoxSlot = RootBox->AddChildToVerticalBox(NarrativePanel))
	{
		VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		VBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	NarrativeScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("NarrativeScrollBox"));
	NarrativeScrollBox->SetAlwaysShowScrollbar(true);
	NarrativePanel->SetContent(NarrativeScrollBox);

	NarrativeBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NarrativeVBox"));
	NarrativeScrollBox->AddChild(NarrativeBox);

	BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BodyText"));
	BodyText->SetAutoWrapText(true);
	BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.90f, 0.86f, 1.0f)));
	BodyText->SetText(LOCTEXT("BootFallbackBody", "SNE UI BOOTED"));
	if (UVerticalBoxSlot* VBoxSlot = NarrativeBox->AddChildToVerticalBox(BodyText))
	{
		VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	ClueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ClueText"));
	ClueText->SetAutoWrapText(true);
	ClueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.88f, 0.83f, 1.0f)));
	if (UVerticalBoxSlot* VBoxSlot = NarrativeBox->AddChildToVerticalBox(ClueText))
	{
		VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	USizeBox* ActionAreaSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ActionAreaSize"));
	ActionAreaSize->SetMinDesiredHeight(190.0f);
	ActionAreaSize->SetMaxDesiredHeight(260.0f);
	if (UVerticalBoxSlot* VBoxSlot = RootBox->AddChildToVerticalBox(ActionAreaSize))
	{
		VBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UBorder* ActionPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ActionPanel"));
	ActionPanel->SetBrushColor(FLinearColor(0.17f, 0.14f, 0.12f, 0.97f));
	ActionPanel->SetPadding(FMargin(8.0f));
	ActionAreaSize->SetContent(ActionPanel);

	UVerticalBox* ActionRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ActionRoot"));
	ActionPanel->SetContent(ActionRoot);

	ActionHeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActionHeaderText"));
	ActionHeaderText->SetText(LOCTEXT("ActionHeaderDefault", "Choices"));
	ActionHeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.84f, 0.66f, 1.0f)));
	if (UVerticalBoxSlot* VBoxSlot = ActionRoot->AddChildToVerticalBox(ActionHeaderText))
	{
		VBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	UScrollBox* ChoiceScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ChoiceScrollBox"));
	ChoiceScrollBox->SetOrientation(EOrientation::Orient_Horizontal);
	ChoiceScrollBox->SetAlwaysShowScrollbar(true);
	if (UVerticalBoxSlot* VBoxSlot = ActionRoot->AddChildToVerticalBox(ChoiceScrollBox))
	{
		VBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	ChoiceListBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ChoiceListBox"));
	ChoiceScrollBox->AddChild(ChoiceListBox);

	for (int32 Index = 0; Index < 6; ++Index)
	{
		USizeBox* ChoiceSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ChoiceSizeBox%d"), Index));
		ChoiceSizeBox->SetMinDesiredWidth(250.0f);

		UButton* ChoiceButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("ChoiceButton%d"), Index));
		UTextBlock* ChoiceLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ChoiceLabel%d"), Index));
		ChoiceLabel->SetText(FText::Format(LOCTEXT("ChoicePlaceholderFmt", "Choice {0}"), FText::AsNumber(Index + 1)));
		ChoiceLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ChoiceLabel->SetAutoWrapText(true);
		ChoiceLabel->SetJustification(ETextJustify::Center);
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(ChoiceButton->AddChild(ChoiceLabel)))
		{
			ButtonSlot->SetPadding(FMargin(10.0f, 7.0f, 10.0f, 7.0f));
		}
		ChoiceButton->SetBackgroundColor(FLinearColor(0.24f, 0.24f, 0.24f, 1.0f));
		ChoiceSizeBox->SetContent(ChoiceButton);
		if (UHorizontalBoxSlot* HSlot = ChoiceListBox->AddChildToHorizontalBox(ChoiceSizeBox))
		{
			HSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			HSlot->SetVerticalAlignment(VAlign_Fill);
		}
		ChoiceButtons.Add(ChoiceButton);
		ChoiceLabels.Add(ChoiceLabel);
	}
}

void USNEGameRootWidget::WireChoiceHandlers()
{
	if (ChoiceButtons.Num() < 6)
	{
		return;
	}

	ChoiceButtons[0]->OnClicked.Clear();
	ChoiceButtons[0]->OnClicked.AddDynamic(this, &USNEGameRootWidget::HandleChoice0);
	ChoiceButtons[1]->OnClicked.Clear();
	ChoiceButtons[1]->OnClicked.AddDynamic(this, &USNEGameRootWidget::HandleChoice1);
	ChoiceButtons[2]->OnClicked.Clear();
	ChoiceButtons[2]->OnClicked.AddDynamic(this, &USNEGameRootWidget::HandleChoice2);
	ChoiceButtons[3]->OnClicked.Clear();
	ChoiceButtons[3]->OnClicked.AddDynamic(this, &USNEGameRootWidget::HandleChoice3);
	ChoiceButtons[4]->OnClicked.Clear();
	ChoiceButtons[4]->OnClicked.AddDynamic(this, &USNEGameRootWidget::HandleChoice4);
	ChoiceButtons[5]->OnClicked.Clear();
	ChoiceButtons[5]->OnClicked.AddDynamic(this, &USNEGameRootWidget::HandleChoice5);
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

USNEDialogueGameSubsystem* USNEGameRootWidget::GetDialogueSubsystem() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI != nullptr ? GI->GetSubsystem<USNEDialogueGameSubsystem>() : nullptr;
}

void USNEGameRootWidget::HandleChoiceClicked(const int32 ChoiceIndex)
{
	if (USNEDialogueGameSubsystem* Subsystem = GetDialogueSubsystem())
	{
		Subsystem->ExecuteChoice(ChoiceIndex);
	}
}

void USNEGameRootWidget::SyncChoiceButtons(const FSNEPresentationData& Data)
{
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
		ChoiceButtons[Index]->SetIsEnabled(Choice.bEnabled);
		ChoiceButtons[Index]->SetBackgroundColor(SNEWidgetInternal::GetChoiceColor(Choice.ChoiceType, Choice.bEnabled));
		ChoiceLabels[Index]->SetColorAndOpacity(FSlateColor(Choice.bEnabled ? FLinearColor::White : FLinearColor(0.75f, 0.75f, 0.75f, 1.0f)));
		ChoiceLabels[Index]->SetText(FText::Format(
			LOCTEXT("ChoiceHotkeyFmt", "[{0}] {1}"),
			FText::AsNumber(Index + 1),
			Choice.Label));
		if (Choice.bEnabled)
		{
			++EnabledChoices;
		}
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
		HeaderText->SetText(LOCTEXT("GameNameTitle", "SELL NO EVIL"));
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

	if (NarrativeScrollBox != nullptr)
	{
		NarrativeScrollBox->ScrollToStart();
	}
}

void USNEGameRootWidget::HandleChoice0()
{
	HandleChoiceClicked(0);
}

void USNEGameRootWidget::HandleChoice1()
{
	HandleChoiceClicked(1);
}

void USNEGameRootWidget::HandleChoice2()
{
	HandleChoiceClicked(2);
}

void USNEGameRootWidget::HandleChoice3()
{
	HandleChoiceClicked(3);
}

void USNEGameRootWidget::HandleChoice4()
{
	HandleChoiceClicked(4);
}

void USNEGameRootWidget::HandleChoice5()
{
	HandleChoiceClicked(5);
}

void USNEGameRootWidget::HandleSubsystemPresentationChanged()
{
	RefreshFromSubsystem();
}

#undef LOCTEXT_NAMESPACE
