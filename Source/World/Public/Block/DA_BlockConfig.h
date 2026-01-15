// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Block/BlockBase.h"
#include "DA_BlockConfig.generated.h"

/**
 * 
 */
UCLASS()
class WORLD_API UDA_BlockConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:
	// Gameplay Tag와 블록 클래스의 매핑 정보
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	TMap<FGameplayTag, TSubclassOf<ABlockBase>> BlockClassMap;
};
