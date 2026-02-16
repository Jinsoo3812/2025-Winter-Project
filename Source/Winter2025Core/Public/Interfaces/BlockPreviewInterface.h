// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BlockPreviewInterface.generated.h"

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
	virtual bool UpdatePreviewState(FVector TargetLocation) = 0;

	/*
	 * 건설 확정 시 호출되는 함수
	 * @return TArray<FTransform> : 소환할 블록들의 태그와 월드 좌표 리스트
	 */
	virtual TArray<FTransform> GetValidSpawnData() = 0;

	/*
	 * 동적으로 다중 블록 형태를 구성할 때 오프셋 배열을 전달하는 함수
	 */
	virtual void SetBlockOffsets(const TArray<FVector>& NewOffsets) = 0;
};
