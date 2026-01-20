// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunkBase.h"
#include "Components/SceneComponent.h"
#include "Async/Async.h"
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

	// Race Condition 방지를 위한 데이터 복사
	// BlockData가 아주 크지 않다면 이 방법이 가장 간단함
	TArray<FBlockData> DataCopy = BlockDataArray;
	int32 SizeX = ChunkSizeX;
	int32 SizeY = ChunkSizeY;
	int32 SizeZ = ChunkSizeZ;
	int32 GridSize = BlockGridSize;

	// 스레드 동작 중 this 객체가 파괴될 수 있으므로 약한 참조 생성
	TWeakObjectPtr<AChunkBase> WeakThis(this);

	/*
	* 엔진이 관리하는 스레드 풀에서 남는 스레드를 하나 잡아 람다 함수를 실행하도록 시킴
	* 스레드 내부에서는 UObject를 다루거나 엔진 관련 함수를 호출해서는 안됨 (ex. GetWorld(), AddInstance 등)
	* 순수 데이터를 다루는 수학 계산 등에만 사용
	*/
	Async(EAsyncExecution::ThreadPool, [WeakThis, DataCopy, SizeX, SizeY, SizeZ, GridSize, MyRequestID]()
		{
			// [Worker Thread] 여기서부터는 별도의 스레드에서 수행됨

			// 배칭 데이터를 담을 임시 맵
			// 매 번 AddInstance를 호출하는 것은 렌더 스레드에게 부담을 줌
			TMap<EBlockType, TArray<FTransform>> LocalBatchData;

			// 많이 사용될 것 같은 블록은 미리 TArray에 메모리 공간을 예약하여 잦은 할당을 방지할 수 있음.
			// LocalBatchData.FindOrAdd(EBlockType::Terrain).Reserve(DataCopy.Num() / 2);

			if(!WeakThis.IsValid())
			{
				// 청크가 파괴되었으므로 작업 중단
				return;
			}
			/*
			* 캐시 적중률을 높이기 위한 3중 반복문
			* 가장 안쪽 루프에 x를 두는 것이 메모리를 순서대로 읽는 방법
			* Index = X + (Y * SizeX) + (Z * SizeX * SizeY)
			*/
			int32 Index = 0;
			for (int32 z = 0; z < SizeZ; z++)
			{
				if (!WeakThis.IsValid())
				{
					// 언제든지 청크가 파괴되었을 수 있으므로 작업 중단
					return;
				}
				// 작업 도중이더라도 자신이 구작업이라는걸 알아채면 즉시 종료
				if (WeakThis->LastUpdateRequestID != MyRequestID)
				{
					return; // 퇴근~
				}

				for (int32 y = 0; y < SizeY; y++)
				{
					for (int32 x = 0; x < SizeX; x++)
					{
						FBlockData CurrentBlock = DataCopy[Index];

						// 공기(None)면 건너뜀
						if (CurrentBlock.Type == EBlockType::None)
						{
							Index++;
							continue;
						}

						// ---------------------------------------------------------
						// Face Culling (숨겨진 블록은 렌더링을 하지 않기 위해)
						// ---------------------------------------------------------
						bool bIsVisible = false;

						// 검사할 6방향 이웃 좌표와 해당 면의 방향(Direction)
						struct FNeighborCheck {
							FIntVector Offset;
							EBlockNeighbor Dir;
						};

						FNeighborCheck Checks[] = {
							{ FIntVector(0, 0, 1),  EBlockNeighbor::Up },
							{ FIntVector(0, 0, -1), EBlockNeighbor::Down },
							{ FIntVector(0, 1, 0),  EBlockNeighbor::Right },
							{ FIntVector(0, -1, 0), EBlockNeighbor::Left },
							{ FIntVector(1, 0, 0),  EBlockNeighbor::Front },
							{ FIntVector(-1, 0, 0), EBlockNeighbor::Back }
						};

						for (const auto& Check : Checks)
						{
							int32 NX = x + Check.Offset.X;
							int32 NY = y + Check.Offset.Y;
							int32 NZ = z + Check.Offset.Z;

							// 내 청크 범위 안인 경우 
							if (NX >= 0 && NX < SizeX && NY >= 0 && NY < SizeY && NZ >= 0 && NZ < SizeZ)
							{
								// 이웃 블록이 유효하며 None(투명)인지 확인
								int32 NeighborIndex = NX + (NY * SizeX) + (NZ * SizeX * SizeY);
								if (DataCopy.IsValidIndex(NeighborIndex) && DataCopy[NeighborIndex].Type == EBlockType::None)
								{
									bIsVisible = true;
									break;
								}
							}
							// 내 청크 범위를 벗어난 경우 (이웃 블록이 옆 청크인 경우)
							else
							{
								// 해당 방향의 이웃 청크 가져오기
								// (스레드 안전을 위해 WeakThis 체크 필수)
								if (!WeakThis.IsValid()) {
									UE_LOG(LogTemp, Warning, TEXT("ChunkBase: Chunk destroyed during neighbor check."));
									return;
								}

								AChunkBase* NeighborChunk = WeakThis->Neighbors[(int32)Check.Dir].Get();

								if (NeighborChunk)
								{
									// 이웃 청크 기준에서의 좌표로 변환
									// 예: 내 X가 -1이면 -> 이웃의 X는 (SizeX - 1)
									int32 LocalX = (NX + SizeX) % SizeX;
									int32 LocalY = (NY + SizeY) % SizeY;
									int32 LocalZ = NZ; // 높이는 공유한다고 가정 (수직 청크 연결 시 로직 필요)

									// 이웃 청크의 데이터를 확인
									// 주의: NeighborChunk->GetBlockData는 내부 배열에 접근하므로
									// NeighborChunk가 파괴되지 않았는지 확인해야 함.
									FBlockData NeighborBlock = NeighborChunk->GetBlockData(LocalX, LocalY, LocalZ);

									if (NeighborBlock.Type == EBlockType::None)
									{
										bIsVisible = true; // 옆집 블록이 투명하면 내 얼굴을 그려야 함
										break;
									}
								}
								else
								{
									// 이웃이 아예 없으면 (맵의 끝) -> 외벽이므로 그린다.
									bIsVisible = true;
									break;
								}
						}

						// ---------------------------------------------------------
						// 렌더링 대상이면 Transform 계산 후 수집
						// ---------------------------------------------------------
						if (bIsVisible)
						{
							FVector Location(x * GridSize, y * GridSize, z * GridSize);
							FTransform Transform(FRotator::ZeroRotator, Location);

							/*
							* CurrentBlock.Type(Key)를 찾고 연결된 TArray(Value)를 반환
							* Key가 없다면 새로 추가 후 반환
							* 이후 .Add() 로 Transform을 추가
							*/
							LocalBatchData.FindOrAdd(CurrentBlock.Type).Add(Transform);
						}

						Index++;
					}
				}
			}

			// 계산 완료 후 메인 스레드(Game Thread)로 복귀
			// HISM 컴포넌트 조작은 반드시 게임 스레드에서 해야 함
			AsyncTask(ENamedThreads::GameThread, [WeakThis, LocalBatchData, MyRequestID]()
				{
					// [Game Thread] 계산된 데이터를 HISM에 적용

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
				});
		});
}