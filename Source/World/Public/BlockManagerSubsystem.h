// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BlockSystemInterface.h"
#include "BlockManagerSubsystem.generated.h"

class ABlockBase;
class ABlockMapManager;
class UBlockConfig;

/*
* 진행 중인 배치 스폰 작업을 추적하기 위한 구조체
*/
struct FActiveBatchInfo
{
	int32 TotalCount = 0;           // 전체 요청 개수
	int32 ProcessedCount = 0;       // 처리된 개수 (성공+실패)
	TArray<TWeakObjectPtr<AActor>> SpawnedActors; // 성공한 액터들 목록
	FOnBlockBatchSpawnComplete Callback; // 다 끝나면 호출할 함수
};

/*
 * 외부 모듈이 요청한 블록 소환, 파괴, 조작 등을 대신 처리해주는 서브시스템
 */
UCLASS()
class WORLD_API UBlockManagerSubsystem : public UTickableWorldSubsystem, public IBlockSystemInterface
{
	GENERATED_BODY()

	// -----------------------------------------------------------------------------
	// 초기화 및 기본 함수
	// -----------------------------------------------------------------------------
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

	float GetGridSize() const override { return GridSize; }

protected:
	float GridSize = 100.0f;
	
	// ---------------------------------------------------------
	// 블록 소환, 파괴, 조작
	// ---------------------------------------------------------
protected:
	// GameplayTag를 이용해 블록 Actor를 소환하는 함수
	AActor* SpawnBlockByTag(
		FGameplayTag BlockTypeTag,
		FVector Location,
		FRotator Rotation,
		bool bEnableGravity,
		const UBlockSpawnPayload* InPayload = nullptr
	) override;

public:
	// 해당 위치에 블록을 소환할 수 있는지 검사하는 함수
	bool IsLocationOccupied(const FVector& CheckLocation, float CheckGridSize) override;

	// BlockReference로부터 블록의 월드 좌표를 얻는 함수
	FVector GetBlockLocation(const FBlockReference& Ref) override;

	// 대량의 블록 생성 요청을 처리하는 함수 (배치 프로세싱)
	void SpawnBlocksBatch(TArray<FBlockSpawnRequest>& Requests,
		const FOnBlockBatchSpawnComplete& OnComplete = FOnBlockBatchSpawnComplete(),
		const UBlockSpawnPayload* InPayload = nullptr) override;

	// Actor 블록 소환 요청을 한 번에 받아 Queue에 등록하는 함수
	// @param Requests: 소환 요청 배열
	void EnqueueBlockSpawns(const TArray<FBlockSpawnRequest>& Requests);

	// 대상 블록 하나를 하이라이트하는 함수
	void HighlightBlock(const FBlockReference& BlockRef, const FGameplayTag& Tag) override;

	bool IsSpawnQueueEmpty() const { return SpawnQueue.IsEmpty(); }

protected:
	// TQueue는 Thread-Safe를 지원
	// 대기 중인 스폰 요청 Queue
	TQueue<FBlockSpawnRequest> SpawnQueue;

	// 한 프레임에 처리할 최대 스폰 개수
	UPROPERTY(EditDefaultsOnly)
	int32 MaxSpawnsPerFrame = 100;

	// 배치 ID 발급용 카운터
	int32 NextBatchID = 0;

	// 현재 진행 중인 배치 작업 맵 (Key: BatchID)
	TMap<int32, FActiveBatchInfo> ActiveBatches;

	// ---------------------------------------------------------
	// 블록 수집
	// ---------------------------------------------------------
public:
	/* FOverlapResult를 받아 BlockReference만 추출 */
	void GetBlocksFromOverlaps(const TArray<struct FOverlapResult>& Overlaps, TArray<FBlockReference>& OutBlocks) override;

	/* 마우스 위치의 BlockReference를 반환하는 함수 */
	bool GetBlockUnderCursor(const APlayerController* PlayerController, FBlockReference& OutBlockRef) override;

	/* HitResult로부터 BlockReference를 반환하는 함수 */
	bool GetBlockFromHitResult(const FHitResult& HitResult, FBlockReference& OutBlockRef) override;

	// ---------------------------------------------------------
	// 참조 캐싱
	// ---------------------------------------------------------
protected:
	// 청크를 찾기 위한 매니저 참조
	UPROPERTY(Transient)
	ABlockMapManager* MapManager = nullptr;

	// Config DA 캐싱
	UPROPERTY(Transient)
	UBlockConfig* CachedBlockConfig = nullptr;
};
