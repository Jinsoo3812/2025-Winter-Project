// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunkBase.h"
#include "Components/SceneComponent.h"
#include "Async/Async.h"
#include "BlockConfig.h"
#include "BlockManagerSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "BlockGameplayTags.h"

AChunkBase::AChunkBase()
{
	// 청크 자체는 틱을 돌 필요가 없음
	PrimaryActorTick.bCanEverTick = false;

	// Transform을 갖는 기본 컴포넌트
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = RootSceneComponent;

	// 데이터 배열 초기화 (공기로 채움)
	int32 TotalBlocks = ChunkSizeX * ChunkSizeY * ChunkSizeZ;
	BlockDataArray.Init(FBlockData{ EBlockType::None }, TotalBlocks);

	// 이중 버퍼 확보
	HISM_Buffers.AddDefaulted(2);
	CurrentBufferIndex = 0;
}

void AChunkBase::BeginPlay()
{
	Super::BeginPlay();

	if(CachedBlockConfig)
	{
		GridSize = CachedBlockConfig->GridSize;
	}
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

	// 현재 청크의 상태 스냅샷
	FChunkSnapshot Snapshot(BlockDataArray, ChunkSizeX, ChunkSizeY, ChunkSizeZ);

	// 이웃 데이터 복사
	for (int32 i = 0; i < (int32)EBlockNeighbor::Count; i++)
	{
		if (Neighbors[i].IsValid())
		{
			
			// 이웃의 전체 BlockData를 복사하여 스레드 안전성 확보
			// 이웃의 접한 면만 복사하는 방법으로 최적화 가능	
			Snapshot.NeighborDataMap.Add((EBlockNeighbor)i, Neighbors[i]->BlockDataArray);
		}
	}

	// BlockConfig는 UObject(DataAsset)이므로 워커 스레드에서 접근해서는 안됨
	// 또한 BlockConfig를 사용하면 매번 여러 번의 포인터를 거쳐야함 (Config -> BlockDefinitions -> Tag 등)
	// 람다 함수 내부에 캡처해두면 캐시 적중률이 올라감
	TMap<EBlockType, bool> IsActorMap;
	TMap<EBlockType, FGameplayTag> ActorTagMap;

	if (CachedBlockConfig)
	{
		for (const FBlockDefinition& Def : CachedBlockConfig->BlockDefinitions)
		{
			IsActorMap.Add(Def.Type, Def.bIsActor);

			if (Def.bIsActor)
			{
				ActorTagMap.Add(Def.Type, Def.Tag);
			}
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("ChunkBase: BlockConfig is not set!"));
		return;
	}

	// 스레드 동작 중 this 객체가 파괴될 수 있으므로 약한 참조 생성
	TWeakObjectPtr<AChunkBase> WeakThis(this);

	float LocalGridSize = GridSize;
	/*
	* 엔진이 관리하는 스레드 풀에서 남는 스레드를 하나 잡아 람다 함수를 실행하도록 시킴
	* 스레드 내부에서는 UObject를 다뤄서는 안되며 단순 계산 작업만 수행해야 함
	* 청크 내의 모든 블록을 순회하며 그려야 할 블록 선별
	*/
	Async(EAsyncExecution::ThreadPool, [WeakThis, Snapshot, MyRequestID, LocalGridSize, IsActorMap, ActorTagMap]()
	{
		// 블록 타입별로 한 번에 AddInstances 호출을 하기 위한 배치 데이터
		TMap<EBlockType, TArray<FTransform>> LocalBatchData;

		// 이번 청크 갱신에서 발생한 액터 스폰 요청들
		TArray<FBlockSpawnRequest> LocalSpawnRequests;

		// 통계용 변수
		int32 TotalSolidBlocks = 0;
		int32 VisibleBlocks = 0; // 그려짐
		int32 CulledBlocks = 0;  // 가려짐 (컬링됨)

		// 많이 사용될 것 같은 블록은 미리 TArray에 메모리 공간을 예약하여 잦은 할당을 방지할 수 있음.
		// LocalBatchData.FindOrAdd(EBlockType::Terrain).Reserve(DataCopy.Num() / 2);

		if (!WeakThis.IsValid()) return;
		if (WeakThis->LastUpdateRequestID != MyRequestID) return;

		// 캐시 적중률을 높이는 3중 반복문
		for (int32 z = 0; z < Snapshot.SizeZ; z++)
		{
			for (int32 y = 0; y < Snapshot.SizeY; y++)
			{
				for (int32 x = 0; x < Snapshot.SizeX; x++)
				{
					FBlockData CurrentBlock = Snapshot.GetBlockData(x, y, z);

					// 그리지 않아도 되는 블록은 건너뜀
					if (CurrentBlock.Type == EBlockType::None) continue;

					TotalSolidBlocks++;

					// 청크 기준의 로컬 좌표
					FVector Location(x * LocalGridSize, y * LocalGridSize, z * LocalGridSize);

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

						// 안 그려진 이웃이 있으면 자신을 그림
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
								continue;
							}
						}
						// Actor가 아니라면 HISM에 추가
						FTransform Transform(FRotator::ZeroRotator, Location);
						LocalBatchData.FindOrAdd(CurrentBlock.Type).Add(Transform);
					}
					else {
						CulledBlocks++;
					}
				}
			}
		}

		// 어떤 블록들을 그려야 하는지 계산 완료 후 메인 스레드(Game Thread)로 복귀
		// HISM 컴포넌트 조작은 반드시 게임 스레드에서 해야 함
		AsyncTask(ENamedThreads::GameThread, [WeakThis, LocalBatchData, LocalSpawnRequests, MyRequestID, LocalGridSize, /*통계*/TotalSolidBlocks, VisibleBlocks, CulledBlocks]()
		{
			// [Game Thread] 계산된 데이터를 HISM에 적용 및 Actor 스폰 요청 전달

			// 이 시점에서 청크가 파괴되었을 수도 있으므로 유효성 검사 (IsValid)
			if (!WeakThis.IsValid()) UE_LOG(LogTemp, Warning, TEXT("ChunkBase: Chunk destroyed before UpdateChunkVisuals could complete."));
			
			
			// 자신이 최신 작업인지 확인
			// 자신이 작업하는 동안 누군가 새로운 작업을 시작했다면, 이 작업은 쓸모 없음
			if (WeakThis->LastUpdateRequestID != MyRequestID) return;

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
				else UE_LOG(LogTemp, Warning, TEXT("ChunkBase: HISM component missing in BackBuffer for BlockType %d"), (int32)Elem.Key);
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
				else UE_LOG(LogTemp, Warning, TEXT("ChunkBase: No HISM component found in BackBuffer for BlockType %d"), (int32)Type);
			}

			// 새 버퍼(BackBuffer) 활성화
			for (auto& Elem : BackMap)
			{
				if (Elem.Value)
				{
					Elem.Value->SetHiddenInGame(false);
					Elem.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					// 렌더링 강제 업데이트 요청
					// Elem.Value->MarkRenderStateDirty();
				}
			}

			// 지금까지 내린 렌더링 명령(ex. AddInstances, SetHiddenInGame 등)이 모두 처리될 때까지
			// 렌더 스레드를 붙잡아두는 펜스 시작
			WeakThis->RenderFence.BeginFence();

			// 타이머를 돌며 주기적으로 렌더링 명령을 다 마쳤는지 검사
			if (UWorld* World = WeakThis->GetWorld())
			{
				// 기존 타이머가 돌고 있다면 제거
				World->GetTimerManager().ClearTimer(WeakThis->RenderFenceTimerHandle);

				// 타이머에 렌더 펜스 검사 함수 바인딩
				FTimerDelegate TimerDel;
				TimerDel.BindUObject(WeakThis.Get(), &AChunkBase::CheckRenderFence, OldBufferIndex);

				// 0.01초마다 검사
				World->GetTimerManager().SetTimer(WeakThis->RenderFenceTimerHandle, TimerDel, 0.01f, true);
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
				// 메모리 예약은 메모리 할당 오버헤드를 줄여줌
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
					int32 X = FMath::RoundToInt(Req.WorldLocation.X / LocalGridSize);
					int32 Y = FMath::RoundToInt(Req.WorldLocation.Y / LocalGridSize);
					int32 Z = FMath::RoundToInt(Req.WorldLocation.Z / LocalGridSize);

					// 소환이 확정되지 않았는데 플래그를 미리 세우고 있음
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
	int32 X = FMath::RoundToInt(LocalLoc.X / GridSize);
	int32 Y = FMath::RoundToInt(LocalLoc.Y / GridSize);
	int32 Z = FMath::RoundToInt(LocalLoc.Z / GridSize);

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
	int32 X = FMath::RoundToInt(LocalLoc.X / GridSize);
	int32 Y = FMath::RoundToInt(LocalLoc.Y / GridSize);
	int32 Z = FMath::RoundToInt(LocalLoc.Z / GridSize);

	int32 Index = GetBlockIndex(X, Y, Z);
	if (BlockDataArray.IsValidIndex(Index))
	{
		// 플래그를 다시 false로 되돌림
		BlockDataArray[Index].bIsActorSpawned = false;
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
	if (!HISM || HISM->GetOwner() != this) {
		UE_LOG(LogTemp, Warning, TEXT("ChunkBase: HighlightHISMBlock called with invalid component or wrong owner."));
		return;
	}
	if (ItemIndex < 0 || ItemIndex >= HISM->GetInstanceCount()) {
		UE_LOG(LogTemp, Warning, TEXT("ChunkBase: HighlightHISMBlock called with invalid ItemIndex %d."), ItemIndex);
	}
	if (!CachedBlockConfig) {
		UE_LOG(LogTemp, Warning, TEXT("ChunkBase: HighlightHISMBlock called but CachedBlockConfig is null."));
	}

	// -------------------------------------------------------------------------
	// Case 1: 폭탄 하이라이트
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
			HISM->SetCustomDataValue(ItemIndex, CachedBlockConfig->BombCPDIndex, 0.0f, true);

			// 맵이 비었으면 컴포넌트 키 자체도 제거
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
		float NewValue = CurrentCount * CachedBlockConfig->BombIntensityPerCount; // Config 변수명 가정
		HISM->SetCustomDataValue(ItemIndex, CachedBlockConfig->BombCPDIndex, NewValue, true);

		return;
	}

	// -------------------------------------------------------------------------
	// Case 2: 일반 하이라이트 (단순 On/Off)
	// -------------------------------------------------------------------------
	const FBlockCPDInfo* CPDInfo = CachedBlockConfig->HighlightSettings.Find(Tag);

	if (CPDInfo || Tag == TAG_Block_Highlight_None)
	{
		float CPDValue = CPDInfo ? CPDInfo->CPDValue : 0.0f;
		int32 CPDIndex = CPDInfo ? CPDInfo->CPDIndex : 0;

		HISM->SetCustomDataValue(ItemIndex, CPDIndex, CPDValue, true);
	}
	else UE_LOG(LogTemp, Warning, TEXT("ChunkBase: HighlightHISMBlock called with unknown Tag %s."), *Tag.ToString());
}

void AChunkBase::CheckRenderFence(int32 OldBufferIndex)
{
	// IsFenceComplete: BeginFence 시점의 렌더링 명령이 모두 처리 되었는가?
	if (RenderFence.IsFenceComplete())
	{
		// 렌더링 명령이 처리됨 -> 새 버퍼가 화면에 나올 준비가 됨

		// 헌 버퍼(Old) 숨기기
		if (HISM_Buffers.IsValidIndex(OldBufferIndex))
		{
			auto& OldMap = HISM_Buffers[OldBufferIndex];
			for (auto& Elem : OldMap)
			{
				if (Elem.Value)
				{
					Elem.Value->SetHiddenInGame(true);
					Elem.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
				else UE_LOG(LogTemp, Warning, TEXT("ChunkBase: Invalid HISM in OldBuffer during Fence Callback."));
			}
		}
		else UE_LOG(LogTemp, Error, TEXT("ChunkBase: Invalid OldBufferIndex in Fence Callback."));

		// 타이머 종료
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RenderFenceTimerHandle);
		}
	}
	else
	{
		// 아직 렌더링 명령 처리중. 다음 타이머 틱에서 다시 검사
	}
}
