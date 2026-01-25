#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "BlockCommon.h"
#include "RenderCommandFence.h"
#include "ChunkBase.generated.h"

class UBlockConfig;
class UDA_BlockConfig;

/*
* 워커 스레드에서 안전하게 작업을 처리하기 위한 현재 청크 상태 스냅샷
* 자신의 모든 BlockData, 6방향 이웃의 모든 BlockData 의 "복사본"
*/
struct FChunkSnapshot
{
	// 내 청크 데이터
	TArray<FBlockData> MyData;
	int32 SizeX, SizeY, SizeZ;

	// 6방향 이웃 청크들의 Blockdata 배열
	// 이웃 청크의 BlockData는 '경계면'만 가져오는 것으로 더 최적화 가능
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

	/*
	* 캐시 히트율을 높이기 위해 1차원 배열로 BlockData 저장
	* 3차원 좌표로 데이터 저장 시 메모리 파편화 및 관리 오버헤드
	*/
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
	void SetBlockConfig(const UBlockConfig* InConfig) { CachedBlockConfig = InConfig; }
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

	// EBlockType에 상응하는 블록 별 메시와 GameplayTag 등이 매핑된 데이터 에셋
	UPROPERTY(Transient)
	const UBlockConfig* CachedBlockConfig = nullptr;

	UPROPERTY(Transient)
	const UDA_BlockConfig* BlockConfigDataAsset = nullptr;

	// HISM 인스턴스별 폭탄 부착 개수를 저장하는 맵
	// Key: 컴포넌트 포인터, Value: <인스턴스 인덱스, 개수> 맵
	TMap<UPrimitiveComponent*, TMap<int32, int32>> HISMBombCountMap;

	// HISM 컴포넌트를 블록 타입 별로 2개씩 갖기 위한 배열
	TArray<TMap<EBlockType, UHierarchicalInstancedStaticMeshComponent*>> HISM_Buffers;

	// 현재 화면에 표시 중인 버퍼 인덱스 (0 or 1)
	int32 CurrentBufferIndex = 0;

	// ---------------------------------------------------------
	// 비동기 청크 업데이트 관련
	// ---------------------------------------------------------

	/** 렌더링 스레드가 작업을 마쳤는지 추적하기 위한 펜스 */
	FRenderCommandFence RenderFence;

	/** 펜스 완료 여부를 주기적으로 검사하기 위한 타이머 */
	FTimerHandle RenderFenceTimerHandle;

	/** 펜스 검사 함수 (타이머에 의해 호출) */
	void CheckRenderFence(int32 OldBufferIndex);
};
