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
			const AGameModeBase* ActiveGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode() : nullptr;
			if (const ASNEPrototypeGameMode* SNEGameMode = Cast<ASNEPrototypeGameMode>(ActiveGameMode))
			{
				DialogueSubsystem->SetRootWidgetClass(SNEGameMode->RootWidgetClass);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("SNE: Active GameMode is not ASNEPrototypeGameMode (%s). Using subsystem default root widget class."),
					*GetNameSafe(ActiveGameMode));
			}
			DialogueSubsystem->EnsureUIForPlayerController(this);
			DialogueSubsystem->StartDayIfNeeded();
		}
	}
}
