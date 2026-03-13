// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEPrototypePlayerController.h"

#include "SNEDialogueGameSubsystem.h"
#include "SNEPrototypeGameMode.h"

ASNEPrototypePlayerController::ASNEPrototypePlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ASNEPrototypePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USNEDialogueGameSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<USNEDialogueGameSubsystem>())
		{
			if (const ASNEPrototypeGameMode* SNEGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ASNEPrototypeGameMode>() : nullptr)
			{
				DialogueSubsystem->SetRootWidgetClass(SNEGameMode->RootWidgetClass);
			}
			DialogueSubsystem->EnsureUIForPlayerController(this);
			DialogueSubsystem->StartDayIfNeeded();
		}
	}
}
