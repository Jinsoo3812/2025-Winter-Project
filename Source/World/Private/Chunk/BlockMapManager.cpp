// Fill out your copyright notice in the Description page of Project Settings.


#include "BlockMapManager.h"
#include "BlockConfig.h"
#include "BlockManagerSubsystem.h"

ABlockMapManager::ABlockMapManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABlockMapManager::BeginPlay()
{
	Super::BeginPlay();

	// [추가] 서브시스템에 자신을 등록
	if (UWorld* World = GetWorld())
	{
		if (UBlockManagerSubsystem* Subsystem = World->GetSubsystem<UBlockManagerSubsystem>())
		{
			Subsystem->RegisterMapManager(this);
		}
	}

	// 게임 시작 시 월드 생성
	GenerateWorld();
}

void ABlockMapManager::GenerateWorld()
{
	if (!BlockConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("WorldMapManager: BlockConfig is missing!"));
		return;
	}

	// 1. 청크 액터 스폰 (빈 껍데기)
	SpawnChunks();

	// 2. 지형 데이터 생성 (데이터 채우기)
	GenerateBasicTerrain();

	// 3. 시각적 업데이트 (비동기 렌더링 시작)
	UpdateAllChunks();
}

void ABlockMapManager::SpawnChunks()
{
	UWorld* World = GetWorld();
	if (!World) {
		UE_LOG(LogTemp, Error, TEXT("WorldMapManager: Unable to get UWorld!"));
	}

	// WorldRange 범위만큼 청크 생성
	for (int32 x = 0; x < WorldRangeX; x++)
	{
		for (int32 y = 0; y < WorldRangeY; y++)
		{
			// 청크의 월드 위치 계산
			// 청크 하나의 실제 크기 = GridSize * ChunkSize
			float ChunkWorldSizeX = AChunkBase::ChunkSizeX * AChunkBase::BlockGridSize; // ex. 16 * 100
			float ChunkWorldSizeY = AChunkBase::ChunkSizeY * AChunkBase::BlockGridSize;

			FVector SpawnLoc(x * ChunkWorldSizeX, y * ChunkWorldSizeY, 0.0f);
			FRotator SpawnRot = FRotator::ZeroRotator;

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;

			AChunkBase* NewChunk = World->SpawnActor<AChunkBase>(AChunkBase::StaticClass(), SpawnLoc, SpawnRot, SpawnParams);

			if (NewChunk)
			{
				NewChunk->SetBlockConfig(BlockConfig);
				NewChunk->SetDABlockConfig(BlockConfigDataAsset);

				// Config에 있는 메시 정보를 청크에 등록
				for (const auto& Pair : BlockConfig->BlockDefinitions)
				{
					NewChunk->RegisterBlockMesh(Pair.Key, Pair.Value.Mesh);
				}

				// 맵에 등록
				ChunkMap.Add(FIntPoint(x, y), NewChunk);

// 에디터 빌드일 때만 포함하는 매크로
#if WITH_EDITOR
				// 에디터에서 보기 좋게 폴더 정리 및 라벨링
				NewChunk->SetFolderPath(FName("Chunks"));
				NewChunk->SetActorLabel(FString::Printf(TEXT("Chunk_%d_%d"), x, y));
#endif
			}
		}
	}
}

void ABlockMapManager::GenerateBasicTerrain()
{
	// 모든 청크를 순회하며 바닥 깔기
	for (auto& Pair : ChunkMap)
	{
		AChunkBase* Chunk = Pair.Value;
		if (!Chunk) {
			UE_LOG(LogTemp, Warning, TEXT("WorldMapManager: Null chunk found in ChunkMap!"));
			return;
		}

		/*
		* 청크 내부의 모든 블록을 순회.
		* 단순 연산이므로 큰 오버헤드가 아니지만, 순간적으로 몇 청크씩 생성하는 경우 프레임 드랍이 있을 수 있음.
		*/
		for (int32 x = 0; x < AChunkBase::ChunkSizeX; x++)
		{
			for (int32 y = 0; y < AChunkBase::ChunkSizeY; y++)
			{
				// FloorHeight 칸까지는 Terrain, 그 위는 공기
				for (int32 z = 0; z < AChunkBase::ChunkSizeZ; z++)
				{
					if (z < FloorHeight)
					{
						Chunk->SetBlockType(x, y, z, EBlockType::Terrain);
					}
					else
					{
						Chunk->SetBlockType(x, y, z, EBlockType::None);
					}
				}
			}
		}
	}
}

void ABlockMapManager::UpdateAllChunks()
{
	for (auto& Pair : ChunkMap)
	{
		if (Pair.Value)
		{
			Pair.Value->UpdateChunkVisuals();
		}
	}
}

AChunkBase* ABlockMapManager::GetChunkAtLocation(FVector Location) const
{
	// 월드 좌표 -> 청크 좌표 변환
	float ChunkWorldSizeX = AChunkBase::ChunkSizeX * AChunkBase::BlockGridSize;
	float ChunkWorldSizeY = AChunkBase::ChunkSizeY * AChunkBase::BlockGridSize;

	// 음수 좌표 처리 등을 위해 Floor(내림 후 정수 변환) 사용 권장
	int32 ChunkX = FMath::FloorToInt(Location.X / ChunkWorldSizeX);
	int32 ChunkY = FMath::FloorToInt(Location.Y / ChunkWorldSizeY);

	if (AChunkBase* const* FoundChunk = ChunkMap.Find(FIntPoint(ChunkX, ChunkY)))
	{
		return *FoundChunk;
	}

	return nullptr;
}

void ABlockMapManager::LinkChunkNeighbors()
{
	for (auto& Pair : ChunkMap)
	{
		FIntPoint CurrentCoord = Pair.Key;     // 현재 청크 좌표 (ex: 2, 3)
		AChunkBase* CurrentChunk = Pair.Value;

		if (!CurrentChunk) {
			UE_LOG(LogTemp, Warning, TEXT("WorldMapManager: Null chunk found in ChunkMap during LinkChunkNeighbors!"));
		}

		// X+ (Front) 확인
		if (AChunkBase** Found = ChunkMap.Find(CurrentCoord + FIntPoint(1, 0)))
			CurrentChunk->SetNeighbor(EBlockNeighbor::Front, *Found);

		// X- (Back) 확인
		if (AChunkBase** Found = ChunkMap.Find(CurrentCoord + FIntPoint(-1, 0)))
			CurrentChunk->SetNeighbor(EBlockNeighbor::Back, *Found);

		// Y+ (Right) 확인
		if (AChunkBase** Found = ChunkMap.Find(CurrentCoord + FIntPoint(0, 1)))
			CurrentChunk->SetNeighbor(EBlockNeighbor::Right, *Found);

		// Y- (Left) 확인
		if (AChunkBase** Found = ChunkMap.Find(CurrentCoord + FIntPoint(0, -1)))
			CurrentChunk->SetNeighbor(EBlockNeighbor::Left, *Found);

		// (선택사항) Z축(Up/Down)은 현재 2D 그리드 맵이므로 생략하거나 
		// 3D ChunkMap을 쓴다면 여기서 연결
	}
}

