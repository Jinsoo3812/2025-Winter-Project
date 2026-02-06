// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "BlockPreviewInterface.generated.h"

/*
* 프리뷰 블록 생성 요청 정보 구조체
*/
USTRUCT(BlueprintType)
struct FPreviewSpawnData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag BlockTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform SpawnTransform; // 월드 트랜스폼
};

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UBlockPreviewInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 프리뷰 블록 BP가 구현할 인터페이스
 */
class WINTER2025CORE_API IBlockPreviewInterface
{
	GENERATED_BODY()

public:
	/*
	 * 매 틱마다 호출되어 프리뷰 블록의 위치와 회전을 갱신하는 함수
	 * @param TargetLocation: 마우스 커서가 가리키는 월드 좌표 (스냅된 위치)
	 * @return bool: 커서가 가리키는 '기준점(Anchor)'이 설치 가능한지 여부
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Preview")
	virtual bool UpdatePreviewState(FVector TargetLocation);

	/*
	 * 건설 확정 시 호출되는 함수
	 * @return TArray<FPreviewSpawnData> : 소환할 블록들의 태그와 월드 좌표 리스트
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Preview")
	virtual TArray<FPreviewSpawnData> GetValidSpawnData();
};
