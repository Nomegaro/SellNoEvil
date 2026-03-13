// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNEPrototypeGameMode.h"

#include "SNEGameRootWidget.h"
#include "SNEPrototypePlayerController.h"

ASNEPrototypeGameMode::ASNEPrototypeGameMode()
{
	PlayerControllerClass = ASNEPrototypePlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	RootWidgetClass = USNEGameRootWidget::StaticClass();
}
