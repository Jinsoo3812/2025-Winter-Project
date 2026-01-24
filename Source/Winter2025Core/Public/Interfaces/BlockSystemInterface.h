

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "BlockSystemInterface.generated.h"

// Actor일 수도 있고, Chunk 내의 HISM 인스턴스일 수도 있음
USTRUCT(BlueprintType)
struct FBlockReference
{
	GENERATED_BODY()

	// 청크 액터 혹은 개별 블록 액터
	UPROPERTY()
	UObject* TargetObject = nullptr;

	// 충돌 검사에서 걸린 컴포넌트 (HISM Component)
	UPROPERTY()
	UPrimitiveComponent* TargetComponent = nullptr;

	// HISM 인덱스 (개별 액터인 경우 -1)
	UPROPERTY()
	int32 ItemIndex = -1;

	bool IsValid() const { return TargetObject != nullptr; }

	// == 연산자 오버로딩 (TArray.Find 등에서 사용)
	bool operator==(const FBlockReference& Other) const
	{
		return TargetObject == Other.TargetObject && ItemIndex == Other.ItemIndex;
	}
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

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// 블록 소환
	virtual AActor* SpawnBlockByTag(FGameplayTag BlockTypeTag, FVector Location, FRotator Rotation, bool bEnableGravity) = 0;
	virtual bool IsLocationOccupied(const FVector& CheckLocation, float CheckGridSize) = 0;

	// 시스템이 Ref를 해석해서 위치를 반환 (HISM이면 Transform 계산, Actor면 ActorLocation)
	virtual FVector GetBlockLocation(const FBlockReference& Ref) = 0;
	virtual float GetGridSize() const = 0;

	// 반경 내의 모든 블록(Actor + HISM) 데이터를 삭제 (블록 파괴)
	virtual void DestroyBlocksInRadius(const FVector& Origin, float Radius) = 0;

	// 반경 내의 블록 참조들을 수집 (스킬 범위 표시 및 타게팅)
	virtual void GetBlocksInRadius(const FVector& Origin, float Radius, TArray<FBlockReference>& OutBlocks) = 0;

	// 특정 블록에 하이라이트 적용 (HISM 인덱스 처리 포함)
	virtual void HighlightBlock(const FBlockReference& BlockRef, const FGameplayTag& Tag) = 0;

	// 서브시스템이 생성될 때(Initialize) 자신을 등록
	static void RegisterSystem(UWorld* World, IBlockSystemInterface* System);

	// 서브시스템이 파괴될 때(Deinitialize) 등록 해제
	static void UnregisterSystem(UWorld* World);

	static IBlockSystemInterface* Get(const UObject* WorldContextObject);

private:
	// 월드별로 인터페이스 구현체를 저장하는 맵
	// Core 모듈의 cpp 파일에 정의될 예정
	static TMap<TWeakObjectPtr<UWorld>, IBlockSystemInterface*> GlobalSystemMap;
};
