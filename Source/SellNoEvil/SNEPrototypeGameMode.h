// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "SNEPrototypeGameMode.generated.h"

UCLASS()
class SELLNOEVIL_API ASNEPrototypeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASNEPrototypeGameMode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SNE|UI")
	TSubclassOf<UUserWidget> RootWidgetClass;
};
