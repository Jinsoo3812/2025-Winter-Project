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
	// 블록 정의 및 데이터 매핑
	// ----------------------------------------------------------------------------

	// 블록 타입별 정의 배열
	UPROPERTY(EditAnywhere, Category = "Block Config")
	TArray<FBlockDefinition> BlockDefinitions;

	UPROPERTY(EditAnywhere, Category = "Block Config")
	float GridSize = 100.0f;

	// 하이라이트 태그 <-> FBlockCPDInfo 매핑
	UPROPERTY(EditAnywhere, Category = "Block Highlight Config")
	TMap<FGameplayTag, FBlockCPDInfo> HighlightSettings;

	// 엔진이 에셋을 로드한 직후 자동으로 호출되는 함수
	void PostLoad() override;

protected:
	// GameplayTag로 FBlockDefinition을 매핑한 맵
	UPROPERTY(Transient)
	TMap<FGameplayTag, int32> TagToIndexMap;

	// BlockType으로 FBlockDefinition을 매핑한 맵
	UPROPERTY(Transient)
	TMap<EBlockType, int32> TypeToIndexMap;

	// 블록 클래스로 FBlockDefinition을 매핑한 맵
	UPROPERTY(Transient)
	TMap<UClass*, int32> ClassToIndexMap;

	// ----------------------------------------------------------------------------
	// 폭탄 관련 설정
	// ----------------------------------------------------------------------------
public:
	// 폭탄에 의한 CPD 설정 인덱스
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Config")
	int32 BombCPDIndex = 1;

	// 폭탄 하나에 해당하는 CPD 강도 증가량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Config")
	float BombIntensityPerCount = 0.33f;

	// ----------------------------------------------------------------------------
	// 헬퍼 함수
	// ----------------------------------------------------------------------------
public:
	// 태그로 블록 정의 찾기
	const FBlockDefinition* GetBlockDef(const FGameplayTag& Tag) const;

	// 타입으로 블록 정의 찾기
	const FBlockDefinition* GetBlockDef(EBlockType Type) const;

	// 클래스로 블록 정의 찾기
	const FBlockDefinition* GetBlockDef(TSubclassOf<AActor> Class) const;
};
