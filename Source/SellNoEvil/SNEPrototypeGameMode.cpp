// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEPrototypeGameMode.h"

#include "SNEPrototypePlayerController.h"

ASNEPrototypeGameMode::ASNEPrototypeGameMode()
{
	PlayerControllerClass = ASNEPrototypePlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	// RootWidgetClass intentionally left null. Assign a WBP derived from USNEGameRootWidget
	// on a Blueprint subclass of this GameMode (or on the GameMode asset).
}
