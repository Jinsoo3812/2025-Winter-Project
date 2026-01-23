#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "BlockCommon.h"
#include "ChunkBase.generated.h"

class UBlockConfig;
class UDA_BlockConfig;

/*
* 스레드 처리를 위한 데이터 스냅샷 구조체
* UObjcet 등을 포함하지 않고, 순수 데이터만 포함
*/
struct FChunkSnapshot
{
	// 내 청크 데이터
	TArray<FBlockData> MyData;
	int32 SizeX, SizeY, SizeZ;

	// 이웃 청크들의 데이터 복사본 (Key: 방향, Value: 블록 데이터 배열)
	// *최적화 주석: 실제 상용 수준에서는 배열 전체가 아니라 '경계면 한 줄'만 복사하는 것이 성능상 좋습니다.
	// 여기서는 이해를 돕기 위해 전체를 복사합니다.
	TMap<EBlockNeighbor, TArray<FBlockData>> NeighborDataMap;

	// 스냅샷 내부에서 좌표를 통해 블록을 조회하는 헬퍼 함수
	FBlockData GetBlockData(int32 X, int32 Y, int32 Z) const;
};

UCLASS()
class WORLD_API AChunkBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AChunkBase();

protected:
	virtual void BeginPlay() override;

public:	
	// -------------------------------------------------------------------------
	// 상수 정의 (청크 크기)
	// -------------------------------------------------------------------------
	static const int32 ChunkSizeX = 16;
	static const int32 ChunkSizeY = 16;
	static const int32 ChunkSizeZ = 32; // 높이는 필요에 따라 조절
	static const int32 BlockGridSize = 100; // 블록 1개의 언리얼 유닛 크기

	// -------------------------------------------------------------------------
	// 데이터 관리
	// -------------------------------------------------------------------------

	// 청크 내부의 블록 데이터 (1차원 배열로 평탄화하여 관리)
	// 인덱스 = x + (y * SizeX) + (z * SizeX * SizeY)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk Data")
	TArray<FBlockData> BlockDataArray;

	// 이웃한 청크의 포인터 저장
	TWeakObjectPtr<AChunkBase> Neighbors[(int32)EBlockNeighbor::Count];

	// 특정 좌표의 블록 타입 설정
	void SetBlockType(int32 X, int32 Y, int32 Z, EBlockType NewType);

	// 특정 좌표의 블록 데이터 가져오기
	// @return 블록 데이터, 범위 벗어나면 빈 블록 반환
	FBlockData GetBlockData(int32 X, int32 Y, int32 Z) const;

	// 3D 좌표 -> 1D 배열 인덱스 변환
	// @return 인덱스 값, 범위 벗어나면 -1 반환
	int32 GetBlockIndex(int32 X, int32 Y, int32 Z) const;

	void SetNeighbor(EBlockNeighbor Direction, AChunkBase* Neighbor) { Neighbors[(int32)Direction] = Neighbor; }

	// 월드 좌표를 받아 청크 내 블록 좌표로 변환 후 블록 제거 (None)
	void RemoveBlockAtWorldLocation(FVector WorldLocation);

	// -------------------------------------------------------------------------
	// 블록 소환 및 시각화
	// -------------------------------------------------------------------------

	/*
	* 블록 타이별 HISM 컴포넌트 매핑
	* EBlockType::Terrain는 지형 mesh를 담당하는 HISM 컴포넌트가 처리하는 등
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk Visuals")
	TMap<EBlockType, UHierarchicalInstancedStaticMeshComponent*> BlockHISMComponents;

	// [추가] BlockMapManager가 청크 생성 직후 Config를 주입해주어야 함
	void SetBlockConfig(const UBlockConfig* InConfig) { BlockConfig = InConfig; }
	void SetDABlockConfig(const UDA_BlockConfig* InConfig) { BlockConfigDataAsset = InConfig; }

	// 데이터를 기반으로 HISM 인스턴스를 다시 그리는 함수
	// 데이터 변경이 있을 때마다 호출하는 것이 아니라, 변경 사항을 모아서 한 번에 호출 권장
	void UpdateChunkVisuals();

	// 초기 설정 시 블록 타입별 Mesh를 등록하는 함수 (DataAsset 등과 연동 필요)
	void RegisterBlockMesh(EBlockType Type, UStaticMesh* Mesh);

	// 서브시스템에서 스폰 실패 시 호출 (롤백용)
	void OnBlockSpawnFailed(FVector WorldLocation);

	// 외부에서 블록을 소환한 후, 데이터를 설정하게 해주는 헬퍼 함수
	void SetBlockData(int32 X, int32 Y, int32 Z, EBlockType NewType, bool bIsActor);

	// HISM 컴포넌트 포인터를 직접 받아 하이라이트 처리
	void HighlightHISMBlock(UPrimitiveComponent* TargetComp, int32 ItemIndex, FGameplayTag Tag);

private:
	// 루트 컴포넌트
	UPROPERTY(VisibleAnywhere)
	USceneComponent* RootSceneComponent;

	// 비동기 작업의 유효성 검사를 위한 ID
	// 메인 스레드에서만 수정되므로 Atomic 보장은 필요 없음
	int32 LastUpdateRequestID = 0;

	// 블록 속성 조회용 (EBlockType을 통해 GameplayTag를 얻을 수 있음)
	UPROPERTY(Transient)
	const UBlockConfig* BlockConfig = nullptr;

	UPROPERTY(Transient)
	const UDA_BlockConfig* BlockConfigDataAsset = nullptr;

	// HISM 인스턴스별 폭탄 부착 개수를 저장하는 맵
	// Key: 컴포넌트 포인터, Value: <인스턴스 인덱스, 개수> 맵
	TMap<UPrimitiveComponent*, TMap<int32, int32>> HISMBombCountMap;
};
