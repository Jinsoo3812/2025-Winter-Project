

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "BlockCommonTypes.h"
#include "BlockSystemInterface.generated.h"

// Actor 블록 소환을 요청하기 위한 구조체
struct FBlockSpawnRequest
{
	// 소환할 블록 태그
	FGameplayTag BlockTag;

	// 소환 위치
	FVector WorldLocation;

	// 소환 후 중력 활성화 여부
	bool bEnableGravity = false;

	FBlockSpawnRequest() = default;

	FBlockSpawnRequest(FGameplayTag InTag, FVector InLocation, bool bInEnableGravity = false)
		: BlockTag(InTag), WorldLocation(InLocation), bEnableGravity(bInEnableGravity) {}
};

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UBlockSystemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * World 모듈의 기능을 외부(Skill 등)에서 호출하기 위한 통합 인터페이스
 */
class WINTER2025CORE_API IBlockSystemInterface
{
	GENERATED_BODY()

	// -----------------------------------------------------------------------------
	// 초기화 및 기본 함수
	// -----------------------------------------------------------------------------
public:
	// 서브시스템이 생성될 때(Initialize) 자신을 등록
	static void RegisterSystem(UWorld* World, IBlockSystemInterface* System);

	// 서브시스템이 파괴될 때(Deinitialize) 등록 해제
	static void UnregisterSystem(UWorld* World);

	static IBlockSystemInterface* Get(const UObject* WorldContextObject);

private:
	// 월드별로 BlockManagerSubsystem을 저장하는 맵
	static TMap<TWeakObjectPtr<UWorld>, IBlockSystemInterface*> GlobalSystemMap;

	// ---------------------------------------------------------
	// 블록 소환, 파괴, 조작
	// ---------------------------------------------------------
public:
	// GameplayTag를 이용해 블록 Actor를 소환하는 함수
	virtual AActor* SpawnBlockByTag(
		FGameplayTag BlockTypeTag,
		FVector Location,
		FRotator Rotation,
		bool bEnableGravity
	) = 0;
	
	virtual void SpawnBlocksBatch(const TArray<FBlockSpawnRequest>& Requests) = 0;

	// 해당 위치에 블록을 소환할 수 있는지 검사하는 함수
	virtual bool IsLocationOccupied(const FVector& CheckLocation, float CheckGridSize) = 0;

	// BlockReference로부터 블록의 월드 좌표를 얻는 함수
	virtual FVector GetBlockLocation(const FBlockReference& Ref) = 0;
	
	virtual float GetGridSize() const = 0;

	// 특정 블록에 하이라이트 적용 (HISM 인덱스 처리 포함)
	virtual void HighlightBlock(const FBlockReference& BlockRef, const FGameplayTag& Tag) = 0;

	// ---------------------------------------------------------
	// 블록 수집
	// ---------------------------------------------------------
public:
	// FOverlapResult를 받아 BlockReference를 추출
	virtual void GetBlocksFromOverlaps(const TArray<struct FOverlapResult>& Overlaps, TArray<FBlockReference>& OutBlocks) = 0;

	/* 마우스 위치의 블록 정보를 반환하는 함수 */
	virtual bool GetBlockUnderCursor(const APlayerController* PlayerController, FBlockReference& OutBlockRef) = 0;

	/* 이미 HitResult가 있다면 그것을 분석하는 함수 */
	virtual bool GetBlockFromHitResult(const FHitResult& HitResult, FBlockReference& OutBlockRef) = 0;
};
