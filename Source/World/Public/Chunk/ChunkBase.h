#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "BlockCommon.h"
#include "ChunkBase.generated.h"

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

	// -------------------------------------------------------------------------
	// 렌더링 (HISM)
	// -------------------------------------------------------------------------

	/*
	* 블록 타이별 HISM 컴포넌트 매핑
	* EBlockType::Terrain는 지형 mesh를 담당하는 HISM 컴포넌트가 처리하는 등
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chunk Visuals")
	TMap<EBlockType, UHierarchicalInstancedStaticMeshComponent*> BlockHISMComponents;

	// 데이터를 기반으로 HISM 인스턴스를 다시 그리는 함수
	// 데이터 변경이 있을 때마다 호출하는 것이 아니라, 변경 사항을 모아서 한 번에 호출 권장
	void UpdateChunkVisuals();

	// 초기 설정 시 블록 타입별 Mesh를 등록하는 함수 (DataAsset 등과 연동 필요)
	void RegisterBlockMesh(EBlockType Type, UStaticMesh* Mesh);

private:
	// 루트 컴포넌트
	UPROPERTY(VisibleAnywhere)
	USceneComponent* RootSceneComponent;

	// 비동기 작업의 유효성 검사를 위한 ID
	// 메인 스레드에서만 수정되므로 Atomic 보장은 필요 없음
	int32 LastUpdateRequestID = 0;
};
