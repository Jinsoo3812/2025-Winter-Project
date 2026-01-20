// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BlockSpawnInterface.h"
#include "BlockManagerSubsystem.generated.h"

class ABlockBase;

/**
 * 
 */
UCLASS()
class WORLD_API UBlockManagerSubsystem : public UWorldSubsystem, public IBlockSpawnInterface
{
	GENERATED_BODY()
	
public:
	// 서브시스템 초기화 함수 (엔진에 의해 호출)
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	// 지정된 위치가 점유되어 있는지 확인하는 헬퍼 함수
	// @param World: 체크할 월드
	// @param CheckLocation: 체크할 위치
	// @param CheckGridSize: 블록의 그리드 크기
	// @return 점유되어 있으면 true, 비어있으면 false
	// @note 프리뷰 블록(ECC_GameTraceChannel1)은 점유 판정에서 제외됨
	bool IsLocationOccupied(const FVector& CheckLocation, float CheckGridSize) override;

	/*
	* IBlockSpawnInterface 구현: 태그에 맞는 블록을 소환하는 함수
	* @param BlockTypeTag: 소환할 블록의 타입을 나타내는 게임플레이 태그
	* @param Location: 블록을 소환할 위치
	* @param bEnableGravity: 소환된 블록에 중력 적용 여부
	* @return 소환된 블록 액터의 포인터, 실패 시 nullptr 반환
	*/
	AActor* SpawnBlockByTag(FGameplayTag BlockTypeTag, FVector Location, FRotator Rotation, bool bEnableGravity) override;

protected:
	// Gameplay Tag와 블록 클래스의 매핑
	UPROPERTY(EditDefaultsOnly)
	TMap<FGameplayTag, TSubclassOf<ABlockBase>> BlockClassMap;
};
