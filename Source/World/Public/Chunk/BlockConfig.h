// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BlockCommonTypes.h"
#include "BlockConfig.generated.h"

UCLASS()
class WORLD_API UBlockConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	// ----------------------------------------------------------------------------
	// 각종 매핑 정보
	// ----------------------------------------------------------------------------

	// BlockType <-> FBlockDefinition 매핑
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Config")
	TMap<EBlockType, FBlockDefinition> BlockDefinitions;

	// GameplayTag <-> FBlockCPDInfo 매핑
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Highlight Config")
	TMap<FGameplayTag, FBlockCPDInfo> HighlightSettings;

	// ----------------------------------------------------------------------------
	// 폭탄 관련 설정
	// ----------------------------------------------------------------------------

	// 폭탄에 의한 CPD 설정 인덱스
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Config")
	int32 BombCPDIndex = 1;

	// 폭탄 하나에 해당하는 CPD 강도 증가량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Config")
	float BombIntensityPerCount = 0.33f;

	// ----------------------------------------------------------------------------
	// 헬퍼 함수
	// ----------------------------------------------------------------------------

	// EBlockType으로 메시를 가져오는 헬퍼 함수
	UStaticMesh* GetMeshForType(EBlockType Type) const;

	// 블록 타입 GameplayTag로 EBlockType을 찾는 헬퍼 함수
	EBlockType GetBlockTypeByTag(const FGameplayTag& Tag) const;

	// 블록 클래스 타입으로 GameplayTag를 찾는 헬퍼 함수
	FGameplayTag GetBlockTagByClass(TSubclassOf<AActor> InClass) const;

	// 블록 타입 GameplayTag로 클래스 타입을 찾는 헬퍼 함수
	TSubclassOf<AActor> GetBlockClassByTag(const FGameplayTag& Tag) const;

	// 하이라이트 GameplayTag로 FBlockCPDInfo를 찾는 헬퍼 함수
	FBlockCPDInfo GetHighlightInfoByTag(const FGameplayTag& Tag) const;
};
