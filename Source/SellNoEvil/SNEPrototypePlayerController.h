// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SNEPrototypePlayerController.generated.h"

class UUserWidget;

UCLASS()
class SELLNOEVIL_API ASNEPrototypePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASNEPrototypePlayerController();

protected:
	virtual void BeginPlay() override;
};
