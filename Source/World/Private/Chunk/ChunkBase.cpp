// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunkBase.h"
#include "Components/SceneComponent.h"
#include "Async/Async.h"
#include "BlockConfig.h"
#include "BlockManagerSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

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

	// 이미 해당 타입의 HISM이 있다면 메시 교체, 없다면 생성
	if (BlockHISMComponents.Contains(Type))
	{
		BlockHISMComponents[Type]->SetStaticMesh(Mesh);
	}
	else
	{
		// 컴포넌트 동적 생성 이름 지정 (디버깅 용이)
		FString CompName = FString::Printf(TEXT("HISM_%d"), (int32)Type);
		UHierarchicalInstancedStaticMeshComponent* NewHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, FName(*CompName));

		if (NewHISM)
		{
			// RootSceneComponent에 부착
			// ChunkBase 액터가 이동/파괴 될 때 함께 동작하도록 설정
			NewHISM->RegisterComponent();
			NewHISM->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			NewHISM->SetStaticMesh(Mesh);

			// 충돌 설정 (블록 용도에 맞게 조정 필요)
			NewHISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			NewHISM->SetCollisionProfileName(TEXT("Block"));

			// 동적 그림자 연산 여부 (끄면 성능 향상 가능)
			NewHISM->bCastDynamicShadow = true;

			// 카메라로부터 거리가 N 이상 떨어진 개별 HISM 인스턴스는 GPU 렌더링 파이프라인에서 제외
			NewHISM->InstanceStartCullDistance = 10000.0f; // 필요시 조정

			BlockHISMComponents.Add(Type, NewHISM);
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

	// 스레드 안전을 위해 Config의 핵심 정보(Actor 여부, 태그)만 맵으로 추출하여 복사
	// UObject(BlockConfig)를 워커 스레드에서 직접 접근하는 것은 위험함
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
	* 스레드 내부에서는 UObject를 다루거나 엔진 관련 함수를 호출해서는 안됨 (ex. GetWorld(), AddInstance 등)
	* 순수 데이터를 다루는 수학 계산 등에만 사용
	*/
	Async(EAsyncExecution::ThreadPool, [WeakThis, Snapshot, GridSize, MyRequestID, IsActorMap, ActorTagMap]()
		{
			// [Worker Thread] 여기서부터는 별도의 스레드에서 수행됨

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
				return; // 퇴근~
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

						FVector Location(x * GridSize, y * GridSize, z * GridSize);

						// 이번 좌표의 블록이 Actor로 처리되어야 하는지 검사
						if (const bool* bIsActor = IsActorMap.Find(CurrentBlock.Type))
						{
							if (*bIsActor)
							{
								// Actor Tag 찾기
								if (const FGameplayTag* Tag = ActorTagMap.Find(CurrentBlock.Type))
								{
									// 스폰 요청 리스트에 추가 (로컬 좌표)
									LocalSpawnRequests.Add({ Location, *Tag });
								}

								// HISM 배칭은 하지 않음. Actor로 직접 소환할거니까
								// 하지만 EBlockType::None이 아니므로 이웃 블록의 Culling 검사에서는 '막힌 블록'으로 인식됨
								continue;
							}
						}

						// 6면 검사
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

							// 스냅샷에게 이웃 블록이 그려져 있냐고 물어봄
							FBlockData NeighborBlock = Snapshot.GetBlockData(NX, NY, NZ);

							// 안그려져 있으면 자신을 그림
							if (NeighborBlock.Type == EBlockType::None)
							{
								bIsVisible = true;
								break;
							}
						}

						if (bIsVisible)
						{
							// 통계
							VisibleBlocks++;
							FVector SpawnLocation(x * GridSize, y * GridSize, z * GridSize);
							FTransform Transform(FRotator::ZeroRotator, SpawnLocation);
							LocalBatchData.FindOrAdd(CurrentBlock.Type).Add(Transform);
						}
						else {
							// 통계
							CulledBlocks++;
						}
					}
				}
			}

			// 계산 완료 후 메인 스레드(Game Thread)로 복귀
			// HISM 컴포넌트 조작은 반드시 게임 스레드에서 해야 함
			AsyncTask(ENamedThreads::GameThread, [WeakThis, LocalBatchData, LocalSpawnRequests, MyRequestID, /*통계*/TotalSolidBlocks, VisibleBlocks, CulledBlocks]()
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
						UE_LOG(LogTemp, Verbose, TEXT("Discarded old chunk update (ID: %d vs Current: %d)"), MyRequestID, WeakThis->LastUpdateRequestID);
						return;
					}

					// 기존 인스턴스 삭제
					for (auto& Elem : WeakThis->BlockHISMComponents)
					{
						if (Elem.Value)
						{
							Elem.Value->ClearInstances();
						}
					}

					// LocalBatchData에 쌓인 인스턴스 데이터를 한 번에 HISM에 추가
					// 블록 타입의 개수 만큼만 AddInstances 호출하므로 렌더 스레드 부담 감소
					for (const auto& BatchPair : LocalBatchData)
					{
						EBlockType Type = BatchPair.Key;
						const TArray<FTransform>& Transforms = BatchPair.Value;

						if (WeakThis->BlockHISMComponents.Contains(Type))
						{
							// 여기서 한 번에 GPU로 전송!
							WeakThis->BlockHISMComponents[Type]->AddInstances(Transforms, false);
						}
					}

					// 통계 출력
					if (TotalSolidBlocks > 0)
					{
						UE_LOG(LogTemp, Log, TEXT("[Chunk Visual Update] Total: %d | Visible: %d (Rendered) | Culled: %d (Hidden/Optimized)"),
							TotalSolidBlocks, VisibleBlocks, CulledBlocks);
					}

					if (LocalSpawnRequests.Num() > 0)
					{
						// 로컬 좌표 -> 월드 좌표 변환을 위해 현재 청크 위치 가져오기
						FVector ChunkOrigin = WeakThis->GetActorLocation();

						TArray<FBlockSpawnRequest> WorldRequests;
						// 메모리 공간은 한 번에 예약합시다.
						WorldRequests.Reserve(LocalSpawnRequests.Num());

						for (const auto& Req : LocalSpawnRequests)
						{
							FBlockSpawnRequest NewReq;
							NewReq.BlockTag = Req.BlockTag;
							NewReq.WorldLocation = ChunkOrigin + Req.WorldLocation; // 월드 좌표로 변환
							NewReq.OwnerChunk = WeakThis;
							WorldRequests.Add(NewReq);
						}

						
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