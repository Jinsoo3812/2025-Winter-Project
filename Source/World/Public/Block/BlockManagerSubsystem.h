// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BlockSystemInterface.h"
#include "BlockManagerSubsystem.generated.h"

class ABlockBase;
class ABlockMapManager;
class UBlockConfig;

// Actor 블록 소환을 요청하기 위한 구조체
struct FBlockSpawnRequest
{
	// 이 위치에
	FVector WorldLocation;
	// 이 블록을
	FGameplayTag BlockTag;
	// 이 청크에
	TWeakObjectPtr<AChunkBase> OwnerChunk;
};

/**
 * 
 */
UCLASS()
class WORLD_API UBlockManagerSubsystem : public UTickableWorldSubsystem, public IBlockSystemInterface
{
	GENERATED_BODY()
	
public:
	// UTickableWorldSubsystem 인터페이스 구현
	virtual void Tick(float DeltaTime) override;

	// 어떤 객체가 Tick을 얼마나 사용하였는지 통계를 수집하는데 사용되는 고유 식별자를 반환
	virtual TStatId GetStatId() const override;

	// 서브시스템 초기화 함수 (엔진에 의해 호출)
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;
	 
	// BlockMapManager가 BeginPlay 시점에 자신을 등록
	void RegisterMapManager(ABlockMapManager* InManager);

	// ---------------------------------------------------------
	// IBlockSystemInterface 구현
	// ---------------------------------------------------------
	virtual AActor* SpawnBlockByTag(FGameplayTag BlockTypeTag, FVector Location, FRotator Rotation, bool bEnableGravity) override;
	virtual bool IsLocationOccupied(const FVector& CheckLocation, float CheckGridSize) override;

	virtual FVector GetBlockLocation(const FBlockReference& Ref) override;
	virtual float GetGridSize() const override { return 100.0f; }

	virtual void DestroyBlocksInRadius(const FVector& Origin, float Radius) override;
	virtual void GetBlocksInRadius(const FVector& Origin, float Radius, TArray<FBlockReference>& OutBlocks) override;
	virtual void HighlightBlock(const FBlockReference& BlockRef, const FGameplayTag& Tag) override;

	// ---------------------------------------------------------
	// 블록 수집 및 생성
	// ---------------------------------------------------------

	/* FOverlapResult를 받아 BlockReference를 추출 */
	void GetBlocksFromOverlaps(const TArray<struct FOverlapResult>& Overlaps, TArray<FBlockReference>& OutBlocks) override;

	/* 마우스 위치의 블록 정보를 반환하는 함수 */
	bool GetBlockUnderCursor(const APlayerController* PlayerController, FBlockReference& OutBlockRef) override;

	/* 이미 HitResult가 있다면 그것을 분석하는 함수 */
	bool GetBlockFromHitResult(const FHitResult& HitResult, FBlockReference& OutBlockRef) override;

	// 대량의 블록 생성 요청을 처리하는 함수 (배치 프로세싱)
	void SpawnBlocksBatch(const TArray<FBlockSpawnRequest>& Requests);

	/*
	* ChunkBase로부터 블록 Actor 소환 요청을 받아 Queue에 추가하는 함수
	* @param Requests: 소환 요청 배열
	*/
	void EnqueueBlockSpawns(const TArray<FBlockSpawnRequest>& Requests);
protected:
	// Gameplay Tag와 블록 클래스의 매핑
	UPROPERTY(EditDefaultsOnly)
	TMap<FGameplayTag, TSubclassOf<ABlockBase>> BlockClassMap;

	/*
	* TQueue는 Thread-Safe를 지원
	* 대기 중인 스폰 요청 Queue
	*/
	TQueue<FBlockSpawnRequest> SpawnQueue;

	// 한 프레임에 처리할 최대 스폰 개수
	UPROPERTY(EditDefaultsOnly)
	int32 MaxSpawnsPerFrame = 100;

	// 청크를 찾기 위한 매니저 참조
	UPROPERTY(Transient)
	ABlockMapManager* MapManager = nullptr;

	// Config DA 캐싱
	UPROPERTY(Transient)
	UBlockConfig* CachedBlockConfig = nullptr;
};
