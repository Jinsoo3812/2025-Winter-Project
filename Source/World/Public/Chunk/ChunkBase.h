#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "RenderCommandFence.h"
#include "BlockCommonTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "ChunkBase.generated.h"

class UBlockConfig;
class UDA_BlockConfig;

// 개별 블록의 변경점을 FastArraySerializer로 보내기 위한 구조체
USTRUCT()
struct WORLD_API FBlockNetworkItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 BlockIndex = -1; // 변경된 블록의 1D 인덱스

	UPROPERTY()
	EBlockType BlockType = EBlockType::None; // 새롭게 변경된 블록 타입

	UPROPERTY()
	bool bIsActorSpawned = false; // Actor 소환 여부

	FBlockNetworkItem() {}
	FBlockNetworkItem(int32 InIndex, EBlockType InType, bool bInIsActor)
		: BlockIndex(InIndex), BlockType(InType), bIsActorSpawned(bInIsActor) {
	}

	// 클라이언트에서 데이터가 추가/변경/삭제될 때 엔진이 자동으로 호출하는 콜백 함수들
	void PreReplicatedRemove(const struct FBlockNetworkArray& Serializer);
	void PostReplicatedAdd(const struct FBlockNetworkArray& Serializer);
	void PostReplicatedChange(const struct FBlockNetworkArray& Serializer);
};

// 변경된 아이템들을 관리하고 네트워크로 쏴주는 배열 구조체
USTRUCT()
struct WORLD_API FBlockNetworkArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FBlockNetworkItem> Items; // 실제로 네트워크를 탈 데이터들

	// 이 데이터를 소유한 ChunkBase 포인터 (클라이언트에서 콜백 발생 시 시각 업데이트를 지시하기 위함)
	// (NotReplicated: 포인터 자체는 네트워크로 보낼 필요가 없으므로 제외)
	UPROPERTY(NotReplicated)
	class AChunkBase* OwningChunk = nullptr;

	// 델타(변경점) 직렬화를 수행하도록 엔진에 지시하는 핵심 함수 오버라이드
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FBlockNetworkItem, FBlockNetworkArray>(Items, DeltaParms, *this);
	}
};

// FBlockNetworkArray가 커스텀 델타 직렬화를 사용함을 엔진 타입 시스템에 등록
template<>
struct TStructOpsTypeTraits<FBlockNetworkArray> : public TStructOpsTypeTraitsBase2<FBlockNetworkArray>
{
	enum { WithNetDeltaSerializer = true };
};

/*
* 워커 스레드에서 안전하게 작업을 처리하기 위한 현재 청크 상태 스냅샷
* 자신의 모든 BlockData, 6방향 이웃의 모든 BlockData 의 복사본
*/
struct FChunkSnapshot
{
	// 내 청크 데이터
	TArray<FBlockData> MyData;
	int32 SizeX, SizeY, SizeZ;

	// 6방향 이웃 청크들의 Blockdata 배열
	// 이웃 청크의 BlockData는 '경계면'만 가져오는 것으로 더 최적화 가능
	TMap<EBlockNeighbor, TArray<FBlockData>> NeighborDataMap;

	FChunkSnapshot(TArray<FBlockData> inMyData, int32 inSizeX, int32 inSizeY, int32 inSizeZ)
		: MyData(inMyData), SizeX(inSizeX), SizeY(inSizeY), SizeZ(inSizeZ) {}

	// 스냅샷 내부에서 좌표를 통해 블록을 조회하는 헬퍼 함수
	FBlockData GetBlockData(int32 X, int32 Y, int32 Z) const;
};

UCLASS()
class WORLD_API AChunkBase : public AActor
{
	GENERATED_BODY()

	// -----------------------------------------------------------------------------
	// 초기화 및 기본 정의
	// -----------------------------------------------------------------------------
public:	
	AChunkBase();

	// Config를 기반으로 HISM 컴포넌트들을 셋업하는 함수
	void SetupHISMComponents();

	// 결정론적 맵 생성을 위한 초기 지형 데이터 할당 함수
	void GenerateInitialTerrain();

protected:
	virtual void BeginPlay() override;

	// 블록 타입 별 HISM에 메시를 등록하는 함수
	void RegisterBlockMesh(EBlockType Type, UStaticMesh* Mesh);

	// -------------------------------------------------------------------------
	// 네트워크 관리
	// -------------------------------------------------------------------------
public:
	// 네트워크 레플리케이션 설정 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// 변경된 블록 데이터만 보관하고 동기화할 네트워크 배열
	UPROPERTY(Replicated)
	FBlockNetworkArray NetworkBlockData;

	// -------------------------------------------------------------------------
	// 청크가 갖는 데이터 관리
	// -------------------------------------------------------------------------
public:
	// 청크 하나의 X축 블록 개수
	int32 ChunkSizeX = 16;
	// 청크 하나의 Y축 블록 개수
	int32 ChunkSizeY = 16;
	// 청크 하나의 Z축 블록 개수
	int32 ChunkSizeZ = 32;

	// 청크 크기 초기화 함수
	void InitializeChunkSize(int32 InSizeX, int32 InSizeY, int32 InSizeZ);

	// 블록 한 개의 크기
	float GridSize = 100;

	// 캐시 히트율을 높이기 위해 1차원 배열로 BlockData 저장
	// 3차원 좌표로 데이터 저장 시 메모리 파편화 및 관리 오버헤드
	UPROPERTY()
	TArray<FBlockData> BlockDataArray;

	UPROPERTY()
	// 이웃한 청크의 포인터 저장
	TWeakObjectPtr<AChunkBase> Neighbors[(int32)EBlockNeighbor::Count];

	void SetNeighbor(EBlockNeighbor Direction, AChunkBase* Neighbor) { Neighbors[(int32)Direction] = Neighbor; }

	// BlockMapManager가 청크 생성 직후 BlockConfig를 주입해주어야 함
	void SetBlockConfig(const UBlockConfig* InConfig) { CachedBlockConfig = InConfig; }

protected:
	// 루트 컴포넌트
	UPROPERTY()
	USceneComponent* RootSceneComponent;

	// 각종 매핑 정보가 있는 BlockConfig 캐시
	UPROPERTY(Transient)
	const UBlockConfig* CachedBlockConfig = nullptr;

	// -------------------------------------------------------------------------
	// 좌표를 통한 블록 데이터 접근 함수
	// -------------------------------------------------------------------------
public:
	// 특정 좌표의 블록 타입 설정
	void SetBlockType(int32 X, int32 Y, int32 Z, EBlockType NewType);

	// 특정 좌표의 블록 데이터 가져오기
	// @return 블록 데이터, 범위 벗어나면 빈 블록 반환
	FBlockData GetBlockData(int32 X, int32 Y, int32 Z) const;

	// 특정 좌표의 블록 데이터 설정
	void SetBlockData(int32 X, int32 Y, int32 Z, EBlockType NewType, bool bIsActor);

	// 3D 좌표 -> 1D 배열 인덱스 변환
	// @return 인덱스 값, 범위 벗어나면 -1 반환
	int32 GetBlockIndex(int32 X, int32 Y, int32 Z) const;

	// 월드 좌표를 받아 청크 내 블록 좌표로 변환 후 블록 제거 (None)
	void RemoveBlockAtWorldLocation(FVector WorldLocation);

	// -------------------------------------------------------------------------
	// 청크 내 블록을 렌더링 & 소환
	// -------------------------------------------------------------------------
public:
	// 블록 타입별 HISM 컴포넌트 매핑
	// EBlockType::Terrain는 지형 mesh를 담당하는 HISM 컴포넌트가 처리하는 등
	UPROPERTY()
	TMap<EBlockType, UHierarchicalInstancedStaticMeshComponent*> BlockHISMComponents;

	// 데이터를 기반으로 "HISM 인스턴스"를 다시 그리는 함수
	void UpdateChunkVisuals();

	// 서브시스템에서 스폰 실패 시 호출 (롤백용)
	void OnBlockSpawnFailed(FVector WorldLocation);

	// HISM 컴포넌트의 인스턴스 하나에 하이라이트 효과를 적용하는 함수
	void HighlightHISMBlock(UPrimitiveComponent* TargetComp, int32 ItemIndex, FGameplayTag Tag);

	// -------------------------------------------------------------------------
	// UpdataeChunkVisuals의 비동기 처리 관련
	// -------------------------------------------------------------------------
protected:
	// 비동기 작업의 유효성 검사를 위한 ID
	// 메인 스레드에서만 수정되므로 Atomic 보장은 필요 없음
	int32 LastUpdateRequestID = 0;

	// HISM 인스턴스별 폭탄 부착 개수를 저장하는 맵
	// Key: 컴포넌트 포인터, Value: <인스턴스 인덱스, 개수> 맵
	TMap<UPrimitiveComponent*, TMap<int32, int32>> HISMBombCountMap;

	// HISM 컴포넌트를 블록 타입 별로 2개씩 갖기 위한 배열
	TArray<TMap<EBlockType, UHierarchicalInstancedStaticMeshComponent*>> HISM_Buffers;

	// 현재 화면에 표시 중인 버퍼 인덱스 (0 or 1)
	int32 CurrentBufferIndex = 0;

	// ---------------------------------------------------------
	// 이중 버퍼링을 사용한 비동기 렌더링 처리
	// ---------------------------------------------------------
public:
	FTimerHandle GetRenderFenceTimerHandle() const { return RenderFenceTimerHandle; }

protected:
	/** 렌더링 스레드가 작업을 마쳤는지 추적하기 위한 펜스 */
	FRenderCommandFence RenderFence;

	/** 펜스 완료 여부를 주기적으로 검사하기 위한 타이머 */
	FTimerHandle RenderFenceTimerHandle;

	/** 펜스 검사 함수 (타이머에 의해 호출) */
	void CheckRenderFence(int32 OldBufferIndex);
};
