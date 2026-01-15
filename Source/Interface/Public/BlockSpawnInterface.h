// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "BlockSpawnInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UBlockSpawnInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 블록 생성을 위한 인터페이스
 * WorldSubsystem이 구현하여 블록 생성을 담당함
 */
class INTERFACE_API IBlockSpawnInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual AActor* SpawnBlockByTag(FGameplayTag BlockTypeTag, FVector Location, bool bEnableGravity) = 0;
};
