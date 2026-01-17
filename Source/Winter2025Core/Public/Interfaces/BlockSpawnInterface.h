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
class WINTER2025CORE_API IBlockSpawnInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual AActor* SpawnBlockByTag(FGameplayTag BlockTypeTag, FVector Location, FRotator Rotation, bool bEnableGravity) = 0;

	virtual bool IsLocationOccupied(const FVector& CheckLocation, float CheckGridSize) = 0;

	// -------------------------------------------------------------------------
	// [접근자 패턴] 외부 모듈에서 구현체를 찾기 위한 정적 함수들
	// -------------------------------------------------------------------------

	// 구현체(Subsystem)가 자신을 등록할 때 사용
	static void RegisterSpawner(UWorld* World, IBlockSpawnInterface* Spawner);

	// 구현체가 소멸될 때 등록 해제
	static void UnregisterSpawner(UWorld* World);

	// 외부 모듈(Skill 등)에서 인터페이스를 가져올 때 사용
	static IBlockSpawnInterface* GetBlockManagerSubsystem(const UWorld* World);

private:
	// 월드별로 인터페이스 구현체를 매핑 (PIE 환경에서 클라이언트/서버 월드 구분 필수)
	// TWeakObjectPtr을 키로 사용하여 월드가 사라졌을 때의 안전성 확보 시도
	static TMap<TWeakObjectPtr<UWorld>, IBlockSpawnInterface*> SpawnerMap;
};
