// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunkBase.h"
#include "Components/SceneComponent.h"
#include "Async/Async.h"
#include "BlockConfig.h"
#include "DA_BlockConfig.h"
#include "BlockManagerSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "BlockGameplayTags.h"

AChunkBase::AChunkBase()
{
	// 청크 자체는 틱을 돌 필요가 없음 (최적화)
	PrimaryActorTick.bCanEverTick = false;

	// Transform을 갖는 기본 컴포넌트
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = RootSceneComponent;

	// 데이터 배열 초기화 (공기로 채움)
	int32 TotalBlocks = ChunkSizeX * ChunkSizeY * ChunkSizeZ;
	BlockDataArray.Init(FBlockData{ EBlockType::None }, TotalBlocks);

	// [신규] 버퍼 2개 공간 확보
	HISM_Buffers.AddDefaulted(2);
	CurrentBufferIndex = 0;
}

void AChunkBase::BeginPlay()
{
	Super::BeginPlay();
}

int32 AChunkBase::GetBlockIndex(int32 X, int32 Y, int32 Z) const
{
	// 범위 검사
	if (X < 0 || X >= ChunkSizeX || Y < 0 || Y >= ChunkSizeY || Z < 0 || Z >= ChunkSizeZ)
	{
		return -1;
	}

	return X + (Y * ChunkSizeX) + (Z * ChunkSizeX * ChunkSizeY);
}

void AChunkBase::SetBlockType(int32 X, int32 Y, int32 Z, EBlockType NewType)
{
	int32 Index = GetBlockIndex(X, Y, Z);
	if (Index != -1 && BlockDataArray.IsValidIndex(Index))
	{
		BlockDataArray[Index].Type = NewType;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ChunkBase: Invalid Index Access at %d, %d, %d"), X, Y, Z);
	}
}

FBlockData AChunkBase::GetBlockData(int32 X, int32 Y, int32 Z) const
{
	int32 Index = GetBlockIndex(X, Y, Z);
	if (Index != -1 && BlockDataArray.IsValidIndex(Index))
	{
		return BlockDataArray[Index];
	}

	// 유효하지 않은 범위는 빈 블록(Air) 반환
	return FBlockData{ EBlockType::None };
}

void AChunkBase::RegisterBlockMesh(EBlockType Type, UStaticMesh* Mesh)
{
	if (Type == EBlockType::None || !Mesh)
	{
		return;
	}

	// 동일한 속성을 가진 HISM 컴포넌트 2개 생성 (더블 버퍼링용)
	for (int32 BufferIdx = 0; BufferIdx < 2; BufferIdx++)
	{
		// 이번에 만들 블록 타입 별 HISM 맵
		TMap<EBlockType, UHierarchicalInstancedStaticMeshComponent*>& TargetMap = HISM_Buffers[BufferIdx];

		// 이미 해당 타입으로 HISM이 등록되어 있으면 메쉬만 교체
		if (TargetMap.Contains(Type))
		{
			TargetMap[Type]->SetStaticMesh(Mesh);
		}
		// 이번에 처음 만드는 블록 타입의 HISM 컴포넌트
		else
		{
			// 컴포넌트 이름 구분 (HISM_Terrain_0, HISM_Terrain_1 등)
			FString CompName = FString::Printf(TEXT("HISM_%d_%d"), (int32)Type, BufferIdx);
			UHierarchicalInstancedStaticMeshComponent* NewHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, FName(*CompName));

			if (NewHISM)
			{
				// 컴포넌트를 World에 등록
				NewHISM->RegisterComponent();
				// RootComponent(Chunk의 위치)에 붙여 위치 동기화
				NewHISM->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
				NewHISM->SetStaticMesh(Mesh);
				NewHISM->SetCollisionProfileName(TEXT("Block"));
				NewHISM->bCastDynamicShadow = true;

				// CPD 사용을 위한 CPD 슬롯 설정(넉넉하게)
				NewHISM->NumCustomDataFloats = 8;

				// Stationary: 위치 고정 & 런타임 인스턴스 변경 등 가능
				NewHISM->SetMobility(EComponentMobility::Stationary);

				// 초기 상태 설정
				if (BufferIdx == CurrentBufferIndex)
				{
					// 현재 버퍼: 보임 + 충돌 켬
					NewHISM->SetHiddenInGame(false);
					NewHISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				}
				else
				{
					// 백 버퍼: 숨김 + 충돌 끔
					NewHISM->SetHiddenInGame(true);
					NewHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}

				TargetMap.Add(Type, NewHISM);
			}
		}
	}
}

void AChunkBase::UpdateChunkVisuals()
{
	/*
	* 새로운 작업 요청이 들어왔으므로 ID 증가
	* 이전에 실행되었던 비동기 작업이 완료되어 돌아왔을 때
	* 이 ID와 비교하여 자신이 최신 작업인지 확인할 수 있음
	*/
	LastUpdateRequestID++;

	// 이번 작업의 고유 ID 캡처
	int32 MyRequestID = LastUpdateRequestID;

	FChunkSnapshot Snapshot;
	Snapshot.MyData = BlockDataArray; // 내 데이터 복사
	Snapshot.SizeX = ChunkSizeX;
	Snapshot.SizeY = ChunkSizeY;
	Snapshot.SizeZ = ChunkSizeZ;
	int32 GridSize = BlockGridSize;

	// 이웃 데이터 복사 (존재하는 경우에만)
	for (int32 i = 0; i < (int32)EBlockNeighbor::Count; i++)
	{
		if (Neighbors[i].IsValid())
		{
			/*
			* 이웃의 전체 BlockData를 복사하여 스레드 안전성 확보
			* 이웃의 접한 면만 복사하는 방법으로 최적화 가능
			*/
			Snapshot.NeighborDataMap.Add((EBlockNeighbor)i, Neighbors[i]->BlockDataArray);
		}
	}

	/*
	* BlockConfig는 UObject(DataAsset)이므로 워커 스레드에서 접근해서는 안됨
	* 또한 BlockConfig를 사용하면 매번 여러 번의 포인터를 거쳐야함 (Config -> BlockDefinitions -> Tag 등)
	* 람다 함수 내부에 캡처해두면 캐시 적중률이 올라감
	*/
	TMap<EBlockType, bool> IsActorMap;
	TMap<EBlockType, FGameplayTag> ActorTagMap;

	if (BlockConfig)
	{
		for (const auto& Pair : BlockConfig->BlockDefinitions)
		{
			IsActorMap.Add(Pair.Key, Pair.Value.bIsActor);
			if (Pair.Value.bIsActor)
			{
				ActorTagMap.Add(Pair.Key, Pair.Value.ActorTag);
			}
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("ChunkBase: BlockConfig is not set!"));
		return;
	}

	// 스레드 동작 중 this 객체가 파괴될 수 있으므로 약한 참조 생성
	TWeakObjectPtr<AChunkBase> WeakThis(this);

	/*
	* 엔진이 관리하는 스레드 풀에서 남는 스레드를 하나 잡아 람다 함수를 실행하도록 시킴
	* 스레드 내부에서는 UObject를 다뤄서는 안되며 단순 계산 작업만 수행해야 함
	* 청크 내의 모든 블록을 순회하며 그려야 할 블록 선별
	*/
	Async(EAsyncExecution::ThreadPool, [WeakThis, Snapshot, GridSize, MyRequestID, IsActorMap, ActorTagMap]()
	{
		// 배칭 데이터를 담을 임시 맵
		// 매 번 AddInstance를 호출하는 것은 렌더 스레드에게 부담을 줌
		TMap<EBlockType, TArray<FTransform>> LocalBatchData;

		// 이번 청크 갱신에서 발생한 액터 스폰 요청들
		TArray<FBlockSpawnRequest> LocalSpawnRequests;

		// 통계용 변수
		int32 TotalSolidBlocks = 0;
		int32 VisibleBlocks = 0; // 그려짐
		int32 CulledBlocks = 0;  // 가려짐 (컬링됨)

		// 많이 사용될 것 같은 블록은 미리 TArray에 메모리 공간을 예약하여 잦은 할당을 방지할 수 있음.
		// LocalBatchData.FindOrAdd(EBlockType::Terrain).Reserve(DataCopy.Num() / 2);

		if(!WeakThis.IsValid())
		{
			// 청크가 파괴되었으므로 작업 중단
			return;
		}
		if (WeakThis->LastUpdateRequestID != MyRequestID)
		{
			// 해당 작업이 최신이 아니므로 중단
			return;
		}

		/*
		* 캐시 적중률을 높이기 위한 3중 반복문
		* 가장 안쪽 루프에 x를 두는 것이 메모리를 순서대로 읽는 방법
		* Index = X + (Y * SizeX) + (Z * SizeX * SizeY)
		*/
		for (int32 z = 0; z < Snapshot.SizeZ; z++)
		{
			for (int32 y = 0; y < Snapshot.SizeY; y++)
			{
				for (int32 x = 0; x < Snapshot.SizeX; x++)
				{
					// 스냅샷을 통해 데이터 가져오기 (안전함)
					FBlockData CurrentBlock = Snapshot.GetBlockData(x, y, z);

					// 그리지 않아도 되는 블록은 건너뜀
					if (CurrentBlock.Type == EBlockType::None) continue;

					TotalSolidBlocks++;

					// 청크 기준의 로컬 좌표
					FVector Location(x * GridSize, y * GridSize, z * GridSize);

					// 6면 검사로 현재 블록을 그려야하는지 검사
					bool bIsVisible = false;
					FIntVector Offsets[] = {
						FIntVector(0, 0, 1), FIntVector(0, 0, -1),
						FIntVector(0, 1, 0), FIntVector(0, -1, 0),
						FIntVector(1, 0, 0), FIntVector(-1, 0, 0)
					};

					for (const FIntVector& Offset : Offsets)
					{
						int32 NX = x + Offset.X;
						int32 NY = y + Offset.Y;
						int32 NZ = z + Offset.Z;

						/*
						* 현재 블록으로부터 한 칸 떨어진 이웃 블록의 데이터 가져오기
						* 이웃 청크에 걸쳐있는 경우 이웃에 접근하여 알아서 가져옴
						* 아예 빈 공간이거나 유효하지 않을 경우 None 블록 반환
						*/
						FBlockData NeighborBlock = Snapshot.GetBlockData(NX, NY, NZ);

						// 안그려져 있으면 자신을 그림
						if (NeighborBlock.Type == EBlockType::None)
						{
							bIsVisible = true;
							break;
						}

						// 윗면에 한해서, 이웃이 Actor 블록이면 바닥이 되어주기 위해 자신을 그림
						if (Offset.Z == 1)
						{
							const bool* bIsNeighborActor = IsActorMap.Find(NeighborBlock.Type);
							if (bIsNeighborActor && *bIsNeighborActor)
							{
								bIsVisible = true;
								break;
							}
						}
					}

					// 그려야 한다면 HISM/Actor 분기하여 스폰 처리
					if (bIsVisible)
					{
						// 통계
						VisibleBlocks++;

						// Actor 블록인지 확인
						if (const bool* bIsActor = IsActorMap.Find(CurrentBlock.Type))
						{
							if (*bIsActor)
							{
								// 이미 스폰되었으면 패스, 안 됐으면 요청 배열에 추가
								if (!CurrentBlock.bIsActorSpawned)
								{
									if (const FGameplayTag* Tag = ActorTagMap.Find(CurrentBlock.Type))
									{
										LocalSpawnRequests.Add({ Location, *Tag });
									}
								}

								// Actor니까 HISM에는 넣지 않음 (여기서 루프 끝)
								continue;
							}
						}
						// Actor가 아니라면 HISM에 추가
						FTransform Transform(FRotator::ZeroRotator, Location);
						LocalBatchData.FindOrAdd(CurrentBlock.Type).Add(Transform);
					}
					else {
						// 통계
						CulledBlocks++;
					}
				}
			}
		}

		// 어떤 블록들을 그려야 하는지 계산 완료 후 메인 스레드(Game Thread)로 복귀
		// HISM 컴포넌트 조작은 반드시 게임 스레드에서 해야 함
		AsyncTask(ENamedThreads::GameThread, [WeakThis, LocalBatchData, LocalSpawnRequests, MyRequestID, GridSize, /*통계*/TotalSolidBlocks, VisibleBlocks, CulledBlocks]()
		{
			// [Game Thread] 계산된 데이터를 HISM에 적용 및 Actor 스폰 요청 전달

			// 이 시점에서 청크가 파괴되었을 수도 있으므로 유효성 검사 (IsValid)
			if (!WeakThis.IsValid()) {
				UE_LOG(LogTemp, Warning, TEXT("ChunkBase: Chunk destroyed before UpdateChunkVisuals could complete."));
			}
			
			/*
			* 자신이 최신 작업인지 확인
			* 자신이 작업하는 동안 누군가 새로운 작업을 시작했다면, 이 작업은 쓸모 없음
			*/
			if (WeakThis->LastUpdateRequestID != MyRequestID)
			{
				return;
			}

			// 버퍼 인덱스 교체
			int32 OldBufferIndex = WeakThis->CurrentBufferIndex;
			int32 NewBufferIndex = (OldBufferIndex + 1) % 2;

			auto& FrontMap = WeakThis->HISM_Buffers[OldBufferIndex]; // 현재 보이는 것 (Old)
			auto& BackMap = WeakThis->HISM_Buffers[NewBufferIndex]; // 뒤에서 준비할 것 (New)

			// BackBuffer 준비
			for (auto& Elem : BackMap)
			{
				// 버퍼의 HISM 컴포넌트 유효성 검사 및 초기화
				if (Elem.Value) {
					Elem.Value->ClearInstances();
				}
				else {
					UE_LOG(LogTemp, Warning, TEXT("ChunkBase: HISM component missing in BackBuffer for BlockType %d"), (int32)Elem.Key);
				}
			}

			// BackBuffer 채우기
			for (const auto& BatchPair : LocalBatchData)
			{
				EBlockType Type = BatchPair.Key;
				const TArray<FTransform>& Transforms = BatchPair.Value;

				// BackMap에 해당 타입의 HISM이 없으면 안전하게 건너뜀 (혹은 생성 고려)
				if (UHierarchicalInstancedStaticMeshComponent* Comp = BackMap.FindRef(Type))
				{
					/*
					* @param Transforms: 추가할 인스턴스들의 Transform 배열
					* @param bShouldReturnIndices: 추가된 인스턴스의 인덱스 배열(TArray<int32>)을 반환할지 여부
					* @param bWorldSpace: Transform이 월드 좌표계인지 여부 (아님!!!!)
					* 마지막에 false를 넣으면 알아서 HISM 컴포넌트의 Transform을 더해줌
					*/
					Comp->AddInstances(Transforms, false, false);
				}
				else {
					UE_LOG(LogTemp, Warning, TEXT("ChunkBase: No HISM component found in BackBuffer for BlockType %d"), (int32)Type);
				}
			}

			// 새 버퍼(BackBuffer) 활성화
			for (auto& Elem : BackMap)
			{
				if (Elem.Value)
				{
					Elem.Value->SetHiddenInGame(false);
					Elem.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					// 렌더링 강제 업데이트 요청
					Elem.Value->MarkRenderStateDirty();
				}
			}

			// 헌 버퍼(Front) 숨기기 -> [다음 프레임으로 지연]
			// 이렇게 하면 아주 짧은 순간(1프레임) 두 지형이 겹쳐 보이지만,
			// 빈 공간이 보이는 것보다는 훨씬 낫고, 같은 위치라 티가 안 남.
			if (UWorld* World = WeakThis->GetWorld())
			{
				// 람다 캡처로 헌 맵(FrontMap) 정보를 넘기기 위해 인덱스 사용
				World->GetTimerManager().SetTimerForNextTick([WeakThis, OldBufferIndex]()
				{
					if (!WeakThis.IsValid()) {
						UE_LOG(LogTemp, Warning, TEXT("ChunkBase: Chunk destroyed before hiding old buffer."));
					}

					// 헌 버퍼의 모든 컴포넌트 숨김
					if (WeakThis->HISM_Buffers.IsValidIndex(OldBufferIndex))
					{
						auto& OldMap = WeakThis->HISM_Buffers[OldBufferIndex];
						for (auto& Elem : OldMap)
						{
							if (Elem.Value)
							{
								Elem.Value->SetHiddenInGame(true);
								Elem.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
							}
						}
					}
				});
			}

			// 현재 버퍼가 누구인지 인덱스 업데이트
			WeakThis->CurrentBufferIndex = NewBufferIndex;

			// 통계 출력
			if (TotalSolidBlocks > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Chunk Visual Update] Total: %d | Visible: %d (Rendered) | Culled: %d (Hidden/Optimized)"),
					TotalSolidBlocks, VisibleBlocks, CulledBlocks);
			}

			// HISM 처리가 끝났으므로 액터 스폰 요청 처리
			if (LocalSpawnRequests.Num() > 0)
			{
				FVector ChunkOrigin = WeakThis->GetActorLocation();
				TArray<FBlockSpawnRequest> WorldRequests;
				// 메모리 예약은 메모리 할당 오버헤드를 줄여줘요~
				WorldRequests.Reserve(LocalSpawnRequests.Num());

				// 요청 변환 (Local -> World)
				for (const auto& Req : LocalSpawnRequests)
				{
					FBlockSpawnRequest NewReq;
					NewReq.BlockTag = Req.BlockTag;

					// 월드 좌표로 변환
					NewReq.WorldLocation = ChunkOrigin + Req.WorldLocation;
					NewReq.OwnerChunk = WeakThis;
					WorldRequests.Add(NewReq);

					// ChunkBase의 관리 목록(BlockDataArray)에서 이 블록의 위치를 찾기
					int32 X = FMath::RoundToInt(Req.WorldLocation.X / GridSize);
					int32 Y = FMath::RoundToInt(Req.WorldLocation.Y / GridSize);
					int32 Z = FMath::RoundToInt(Req.WorldLocation.Z / GridSize);

					int32 Index = WeakThis->GetBlockIndex(X, Y, Z);
					if (WeakThis->BlockDataArray.IsValidIndex(Index))
					{
						WeakThis->BlockDataArray[Index].bIsActorSpawned = true;
					}
				}

				// 서브시스템에 액터 생성 요청을 모아서 전달
				if (UWorld* World = WeakThis->GetWorld())
				{
					if (UBlockManagerSubsystem* Subsystem = World->GetSubsystem<UBlockManagerSubsystem>())
					{
						Subsystem->EnqueueBlockSpawns(WorldRequests);
					}
				}
				UE_LOG(LogTemp, Log, TEXT("ChunkBase: Enqueued %d actor spawns."), WorldRequests.Num());
			}
		});
	});
}

FBlockData FChunkSnapshot::GetBlockData(int32 X, int32 Y, int32 Z) const
{
	// 내 청크 범위 내일 경우의 처리
	if (X >= 0 && X < SizeX && Y >= 0 && Y < SizeY && Z >= 0 && Z < SizeZ)
	{
		int32 Index = X + (Y * SizeX) + (Z * SizeX * SizeY);
		if (MyData.IsValidIndex(Index))
		{
			return MyData[Index];
		}
		return FBlockData{ EBlockType::None };
	}

	// 2. 범위를 벗어났다면 어느 이웃인지 판별
	EBlockNeighbor TargetDir = EBlockNeighbor::Count;
	int32 LocalX = X;
	int32 LocalY = Y;
	int32 LocalZ = Z;

	// X축 검사
	if (X < 0)
	{
		TargetDir = EBlockNeighbor::Back;
		LocalX = (X + SizeX) % SizeX; // -1 -> 15
	}
	else if (X >= SizeX)
	{
		TargetDir = EBlockNeighbor::Front;
		LocalX = (X + SizeX) % SizeX; // 16 -> 0
	}

	// Y축 검사 (X축이 범위 안일 때만 체크)
	else if (Y < 0)
	{
		TargetDir = EBlockNeighbor::Left;
		LocalY = (Y + SizeY) % SizeY;
	}
	else if (Y >= SizeY)
	{
		TargetDir = EBlockNeighbor::Right;
		LocalY = (Y + SizeY) % SizeY;
	}
	// Z축 검사 생략 (단층 맵이므로)

	// 이웃 데이터가 스냅샷에 존재하는지 확인
	if (TargetDir != EBlockNeighbor::Count && NeighborDataMap.Contains(TargetDir))
	{
		const TArray<FBlockData>& NeighborArr = NeighborDataMap[TargetDir];
		int32 Index = LocalX + (LocalY * SizeX) + (LocalZ * SizeX * SizeY);

		if (NeighborArr.IsValidIndex(Index))
		{
			return NeighborArr[Index];
		}
	}

	// 이웃이 없거나 데이터가 없으면 '투명(None)' 취급 -> 그래야 외벽이 그려짐
	return FBlockData{ EBlockType::None };
}

void AChunkBase::RemoveBlockAtWorldLocation(FVector WorldLocation)
{
	// 월드 좌표를 청크 로컬 좌표로 변환 (청크의 회전이 없다고 가정 시 단순 빼기)
	FVector LocalLoc = WorldLocation - GetActorLocation();

	// 그리드 좌표로 변환
	int32 X = FMath::RoundToInt(LocalLoc.X / BlockGridSize);
	int32 Y = FMath::RoundToInt(LocalLoc.Y / BlockGridSize);
	int32 Z = FMath::RoundToInt(LocalLoc.Z / BlockGridSize);

	// 데이터 갱신 (None으로 변경)
	// SetBlockType 내부에서 유효성 검사(Index Check)를 하므로 안전함
	SetBlockType(X, Y, Z, EBlockType::None);

	// 시각적 업데이트 요청 (이 블록은 Actor였으므로 HISM 갱신은 필요 없을 수 있으나, 
	// 이웃 블록의 옆면(Culling)을 다시 그려야 하므로 호출 필수
	UpdateChunkVisuals();

	// 통계
	UE_LOG(LogTemp, Warning, TEXT("[ChunkBase] Block Data cleared! Triggering Visual Update to reveal neighbors..."));
}

void AChunkBase::OnBlockSpawnFailed(FVector WorldLocation)
{
	FVector LocalLoc = WorldLocation - GetActorLocation();
	int32 X = FMath::RoundToInt(LocalLoc.X / BlockGridSize);
	int32 Y = FMath::RoundToInt(LocalLoc.Y / BlockGridSize);
	int32 Z = FMath::RoundToInt(LocalLoc.Z / BlockGridSize);

	int32 Index = GetBlockIndex(X, Y, Z);
	if (BlockDataArray.IsValidIndex(Index))
	{
		// 플래그를 다시 false로 되돌림 -> 다음 UpdateChunkVisuals 때 다시 시도하게 됨
		BlockDataArray[Index].bIsActorSpawned = false;

		// 필요하다면 다시 시각적 업데이트를 요청하거나, 
		// 일정 시간 뒤에 재시도하도록 로직을 추가할 수 있음
		// UE_LOG(LogTemp, Warning, TEXT("ChunkBase: Spawn Failed Rollback at %d %d %d"), X, Y, Z);
	}
}

void AChunkBase::SetBlockData(int32 X, int32 Y, int32 Z, EBlockType NewType, bool bIsActor)
{
	int32 Index = GetBlockIndex(X, Y, Z);
	if (Index != -1 && BlockDataArray.IsValidIndex(Index))
	{
		BlockDataArray[Index].Type = NewType;
		BlockDataArray[Index].bIsActorSpawned = bIsActor;
	}
}

void AChunkBase::HighlightHISMBlock(UPrimitiveComponent* TargetComp, int32 ItemIndex, FGameplayTag Tag)
{
	// 1. 컴포넌트 및 인덱스 유효성 검사
	UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(TargetComp);
	if (!HISM || HISM->GetOwner() != this) return;
	if (ItemIndex < 0 || ItemIndex >= HISM->GetInstanceCount()) return;
	if (!BlockConfigDataAsset) return;

	// -------------------------------------------------------------------------
	// Case 1: 폭탄 하이라이트 (중첩 카운팅 로직)
	// -------------------------------------------------------------------------
	if (Tag.MatchesTag(TAG_Block_Highlight_Bomb))
	{
		// 해당 컴포넌트의 카운트 맵을 가져오거나 생성
		TMap<int32, int32>& InstanceCounts = HISMBombCountMap.FindOrAdd(HISM);

		// [초기화] Bomb_None 태그가 오면 카운트 리셋 및 하이라이트 끄기
		if (Tag.MatchesTag(TAG_Block_Highlight_Bomb_None))
		{
			InstanceCounts.Remove(ItemIndex); // 맵에서 데이터 삭제 (메모리 절약)

			// CPD 0으로 초기화
			HISM->SetCustomDataValue(ItemIndex, BlockConfigDataAsset->BombCPDIndex, 0.0f, true);

			// 맵이 비었으면 컴포넌트 키 자체도 제거 (선택사항)
			if (InstanceCounts.Num() == 0)
			{
				HISMBombCountMap.Remove(HISM);
			}
			return;
		}

		// [증가] 현재 개수 가져오기 (없으면 0)
		int32 CurrentCount = InstanceCounts.FindRef(ItemIndex);

		// 최대 개수 제한 (BlockBase 로직과 동일)
		CurrentCount = FMath::Clamp(CurrentCount + 1, 0, /*임시*/3);

		// 맵에 저장
		InstanceCounts.Add(ItemIndex, CurrentCount);

		// CPD 계산: 개수 * 강도
		float NewValue = CurrentCount * BlockConfigDataAsset->BombIntensityPerCount; // Config 변수명 가정
		HISM->SetCustomDataValue(ItemIndex, BlockConfigDataAsset->BombCPDIndex, NewValue, true);

		return;
	}

	// -------------------------------------------------------------------------
	// Case 2: 일반 하이라이트 (단순 On/Off)
	// -------------------------------------------------------------------------
	const FBlockCPDInfo* CPDInfo = BlockConfigDataAsset->BlockCPDIndexMap.Find(Tag);

	// 태그를 찾았거나, None(해제) 태그인 경우 처리
	if (CPDInfo || Tag == TAG_Block_Highlight_None)
	{
		float CPDValue = CPDInfo ? CPDInfo->CPDValue : 0.0f;
		int32 CPDIndex = CPDInfo ? CPDInfo->CPDIndex : 0; // None일 땐 0번 인덱스(Color)를 0.0으로 끈다고 가정

		// 태그가 None이면 값을 0으로 강제
		if (Tag == TAG_Block_Highlight_None)
		{
			CPDValue = 0.0f;
			// 보통 하이라이트용 CPD 인덱스를 알아야 하는데, 
			// 여기서는 Preview나 Select 등 모든 하이라이트를 끈다고 가정하고 0번이나 특정 인덱스를 사용
			// 정확히 하려면 '어떤 하이라이트를 끌 것인가'에 대한 정보가 더 필요하지만,
			// 보통 단일 채널을 쓴다면 0번 인덱스를 0.0f로 미는 것으로 충분함.
		}

		HISM->SetCustomDataValue(ItemIndex, CPDIndex, CPDValue, true);
	}
}