// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Block/BlockBase.h"
#include "BlockCommonTypes.h"
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

	// Gameplay Tag와 CPD index 및 조정값 매핑 정보
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	TMap<FGameplayTag, FBlockCPDInfo> BlockCPDIndexMap;

	// 폭탄 효과를 적용할 CPD 인덱스
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
	int32 BombCPDIndex = 1; 

	// 폭탄 1개당 증가할 CPD 값 (예: 0.2, 1.0 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
	float BombIntensityPerCount = 0.33f; 
};
