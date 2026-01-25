// Fill out your copyright notice in the Description page of Project Settings.


#include "Block/BlockManagerSubsystem.h"
#include "CollisionChannels.h"
#include "Block/BlockSettings.h"
#include "Block/BlockBase.h"
#include "Chunkbase.h"
#include "BlockMapManager.h"
#include "BlockConfig.h"
#include "Engine/OverlapResult.h"
#include "GameplayEventInterface.h"

void UBlockManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	IBlockSystemInterface::RegisterSystem(GetWorld(), this);

	// 개발자 설정(Project Settings)에서 설정 객체 가져오기
	// GetDefault<T>()는 CDO(Class Default Object)를 가져오므로 매우 빠름
	const UBlockSettings* Settings = GetDefault<UBlockSettings>();

	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("BlockManagerSubsystem: Failed to get BlockSettings."));
		return;
	}

	// 설정에 할당된 데이터 에셋이 있는지 확인
	if (Settings->BlockConfigAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockManagerSubsystem: BlockConfigAsset is not set in Project Settings -> Game -> Block System."));
		return;
	}

	// Soft Pointer를 동기 로드(Synchronous Load)하여 실제 객체 가져오기
	// 초기화 단계이므로 동기 로드가 허용됨. 
	CachedBlockConfig = Settings->BlockConfigAsset.LoadSynchronous();

	if(!CachedBlockConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("BlockManagerSubsystem: Failed to load BlockConfig from Asset."));
		return;
	}
}

void UBlockManagerSubsystem::Deinitialize()
{
	// 시스템 종료 시 반드시 등록 해제 (Dangling Pointer 방지)
	IBlockSystemInterface::UnregisterSystem(GetWorld());

	Super::Deinitialize();
}

void UBlockManagerSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 큐가 비어있으면 조기 리턴
	if (SpawnQueue.IsEmpty())
	{
		return;
	}

	int32 ProcessCount = 0;
	FBlockSpawnRequest Request;

	// 정해진 예산(MaxSpawnsPerFrame)만큼만 반복 처리
	while (ProcessCount < MaxSpawnsPerFrame && SpawnQueue.Dequeue(Request))
	{
		// 이미 해당 위치에 블록이 있는지 등은 SpawnBlockByTag 내부의 IsLocationOccupied에서 체크함
		// 하지만 청크 생성 시점이라 비어있을 확률이 높음

		// Actor 스폰 실행 (중력은 끄고 시작하는 것이 일반적, 필요시 true)
		AActor* SpawnedActor = SpawnBlockByTag(Request.BlockTag, Request.WorldLocation, FRotator::ZeroRotator, false);
		if (SpawnedActor) {
			if (ABlockBase* NewBlock = Cast<ABlockBase>(SpawnedActor))
			{
				// 요청서에 적힌 청크가 살아있는지 확인
				if (Request.OwnerChunk.IsValid())
				{
					NewBlock->SetParentChunk(Request.OwnerChunk.Get());
				}
				else
				{
					// 청크가 그새 파괴되었거나 정보가 없다면 경고 (디버깅용)
					// UE_LOG(LogTemp, Warning, TEXT("Spawned Block but Chunk is missing!"));
				}
			}
		}
		// 스폰 실패이므로 롤백 요청
		else {
			if (Request.OwnerChunk.IsValid())
			{
				Request.OwnerChunk->OnBlockSpawnFailed(Request.WorldLocation);
			}
		}
		ProcessCount++;
	}
}

// 필수 오버라이드 (Stat ID 반환)
TStatId UBlockManagerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBlockManagerSubsystem, STATGROUP_Tickables);
}

void UBlockManagerSubsystem::EnqueueBlockSpawns(const TArray<FBlockSpawnRequest>& Requests)
{
	for (const FBlockSpawnRequest& Req : Requests)
	{
		SpawnQueue.Enqueue(Req);
	}
}

void UBlockManagerSubsystem::RegisterMapManager(ABlockMapManager* InManager)
{
	MapManager = InManager;
}

AActor* UBlockManagerSubsystem::SpawnBlockByTag(FGameplayTag BlockTypeTag, FVector Location, FRotator Rotation, bool bEnableGravity)
{
	if (!CachedBlockConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnBlockByTag: MainConfig is not loaded!"));
		return nullptr;
	}

	// Config의 헬퍼 함수를 통해 클래스 조회 (GetBlockClassByTag)
	TSubclassOf<AActor> FoundActorClass = CachedBlockConfig->GetBlockClassByTag(BlockTypeTag);

	if (!FoundActorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockManagerSubsystem: No block class found for tag %s"), *BlockTypeTag.ToString());
		return nullptr;
	}

	// GridSize 가져오기
	float GridSize = 100.0f;
	if (ABlockBase* CDO = Cast<ABlockBase>(FoundActorClass->GetDefaultObject()))
	{
		GridSize = CDO->GetBlockGridSize();
	}

	// 위치 점유 확인
	if (IsLocationOccupied(Location, GridSize))
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockManagerSubsystem: Location %s is occupied"), *Location.ToString());
		return nullptr;
	}

	// 소환
	// 충돌 쿼리는 이미 했으므로 AlwaysSpawn 사용
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewActor = GetWorld()->SpawnActor<AActor>(FoundActorClass, Location, Rotation, SpawnParams);
	ABlockBase* NewBlock = Cast<ABlockBase>(NewActor);

	// 생성 후 설정 (중력, 위치 보정 등)
	if (NewBlock)
	{
		// 위치를 명확하게 다시 설정 (SpawnActor 시 미세한 오차 방지)
		NewBlock->SetActorLocation(Location);

		// 중력 설정: 요청받은 bEnableGravity 값에 따라 블록 상태 변경
		// BlockBase의 변수(bCanFall)와 Tick 설정을 외부에서 제어
		if (bEnableGravity)
		{
			NewBlock->SetCanFall(true); // Setter 함수가 없다면 BlockBase에 추가 권장 또는 public 변수 직접 접근
			NewBlock->SetActorTickEnabled(true);
		}
		else
		{
			NewBlock->SetCanFall(false);
			NewBlock->SetActorTickEnabled(false);
		}

		// 디버그용
		if (!MapManager)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnBlockByTag: MapManager is NULL! Cannot link chunk."));
		}
		else if (!CachedBlockConfig)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnBlockByTag: LoadedBlockConfig is NULL!"));
		}

		/*
		* 청크 시스템과 동기화
		*/
		if (MapManager && CachedBlockConfig)
		{
			if (AChunkBase* TargetChunk = MapManager->GetChunkAtLocation(Location))
			{
				// 태그를 이용해 정확한 EBlockType 찾기
				EBlockType TargetType = CachedBlockConfig->GetBlockTypeByTag(BlockTypeTag);

				// 만약 Config에 없는 태그라면 기본값(Destructible) 혹은 에러 처리
				if (TargetType == EBlockType::None)
				{
					TargetType = EBlockType::Destructible; // Fallback
					UE_LOG(LogTemp, Warning, TEXT("SpawnBlockByTag: Unknown Tag %s, defaulting to Destructible"), *BlockTypeTag.ToString());
				}

				FVector LocalLoc = Location - TargetChunk->GetActorLocation();
				int32 X = FMath::RoundToInt(LocalLoc.X / GridSize);
				int32 Y = FMath::RoundToInt(LocalLoc.Y / GridSize);
				int32 Z = FMath::RoundToInt(LocalLoc.Z / GridSize);

				// 찾은 타입으로 설정
				TargetChunk->SetBlockData(X, Y, Z, TargetType, true);

				NewBlock->SetParentChunk(TargetChunk);
				TargetChunk->UpdateChunkVisuals();
			}
		}
		return NewBlock;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BlockManagerSubsystem: Failed to spawn block at %s"), *Location.ToString());
		return nullptr;
	}
}

void UBlockManagerSubsystem::SpawnBlocksBatch(const TArray<FBlockSpawnRequest>& Requests)
{
	if (!MapManager)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnBlocksBatch: MapManager is not registered!"));
		return;
	}

	// 청크별로 요청 분류
	// Key: 청크 포인터, Value: 해당 청크에 속한 요청들
	TMap<AChunkBase*, TArray<FBlockSpawnRequest>> ChunkRequestMap;

	for (const FBlockSpawnRequest& Req : Requests)
	{
		if (AChunkBase* Chunk = MapManager->GetChunkAtLocation(Req.WorldLocation))
		{
			ChunkRequestMap.FindOrAdd(Chunk).Add(Req);
		}
	}

	// 각 청크별로 데이터 일괄 업데이트 수행
	for (auto& Pair : ChunkRequestMap)
	{
		AChunkBase* Chunk = Pair.Key;
		TArray<FBlockSpawnRequest>& ChunkRequests = Pair.Value;

		for (FBlockSpawnRequest& Req : ChunkRequests)
		{
			// 로컬 좌표 변환
			FVector LocalLoc = Req.WorldLocation - Chunk->GetActorLocation();
			int32 X = FMath::RoundToInt(LocalLoc.X / AChunkBase::BlockGridSize);
			int32 Y = FMath::RoundToInt(LocalLoc.Y / AChunkBase::BlockGridSize);
			int32 Z = FMath::RoundToInt(LocalLoc.Z / AChunkBase::BlockGridSize);

			// 태그로 타입 찾기
			EBlockType TargetType = EBlockType::Destructible; // 기본값
			if (CachedBlockConfig)
			{
				EBlockType FoundType = CachedBlockConfig->GetBlockTypeByTag(Req.BlockTag);
				if (FoundType != EBlockType::None)
				{
					TargetType = FoundType;
				}
			}

			Chunk->SetBlockData(X, Y, Z, TargetType, true);
			Req.OwnerChunk = Chunk;
		}

		// 시각적 업데이트 (마지막에 한 번만 호출!)
		Chunk->UpdateChunkVisuals();
	}

	// 액터 스폰 큐에 등록 (Time Slicing)
	// 데이터는 이미 다 바꿨고, 이제 실제 액터만 천천히 나오면 됨
	EnqueueBlockSpawns(Requests);

	UE_LOG(LogTemp, Log, TEXT("Batch Spawned %d blocks across %d chunks."), Requests.Num(), ChunkRequestMap.Num());
}

bool UBlockManagerSubsystem::IsLocationOccupied(
	const FVector& CheckLocation,
	float CheckGridSize)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("BlockManagerSubsystem: World is null"));
		return true; // 안전을 위해 true 반환 (생성 막음)
	}

	// MakeBox는 인자를 반지름으로 사용함
	// 0.5를 넣으면 100 * 100 * 100 크기의 박스가 되어 꽉 차므로 0.4 사용
	FVector BoxExtent = FVector(CheckGridSize * 0.4f, CheckGridSize * 0.4f, CheckGridSize * 0.4f);
	FCollisionShape CheckShape = FCollisionShape::MakeBox(BoxExtent);

	// ObjectType 기반 쿼리
	// 다른 블록이 있는 곳에는 스폰 불가
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Block);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn); // 플레이어 등도 방해물이 될 수 있음

	FCollisionQueryParams QueryParams;

	// 충돌한 블록들을 반환할 것이 아니므로 OverlapMulti 대신 OverlapAny 사용
	return World->OverlapAnyTestByObjectType(CheckLocation, FQuat::Identity, ObjectQueryParams, CheckShape, QueryParams);
}

void UBlockManagerSubsystem::GetBlocksInRadius(const FVector& Origin, float Radius, TArray<FBlockReference>& OutBlocks)
{
	UWorld* World = GetWorld();
	if (!World) {
		UE_LOG(LogTemp, Error, TEXT("BlockManagerSubsystem: World is null"));
	}

	OutBlocks.Reset();

	// 1. OverlapMulti로 물리적 충돌체 검색
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Block); // Block 채널
	FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	FCollisionQueryParams Params;

	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, Shape, Params);

	float RadiusSq = Radius * Radius;
	for (const FOverlapResult& Result : Overlaps)
	{
		FBlockReference Ref;

		// Case A: HISM (청크 지형)
		if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(Result.GetComponent()))
		{
			if (AChunkBase* Chunk = Cast<AChunkBase>(Result.GetActor()))
			{
				// HISM 컴포넌트를 이용해 즉시 위치 조회
				FTransform InstanceTransform;
				HISM->GetInstanceTransform(Result.ItemIndex, InstanceTransform, true);

				// 반경 내에 있는지 재확인
				if (FVector::DistSquared(Origin, InstanceTransform.GetLocation()) <= RadiusSq)
				{
					Ref.TargetObject = Chunk;
					Ref.TargetComponent = HISM;
					Ref.ItemIndex = Result.ItemIndex;
					OutBlocks.AddUnique(Ref);
				}
			}
		}
		// Case B: Actor (파괴 가능 블록 등)
		else if (AActor* Actor = Result.GetActor())
		{
			// GameplayEventInterface 구현 여부 확인 (상호작용 가능한 블록만)
			if (Actor->Implements<UGameplayEventInterface>())
			{
				if (FVector::DistSquared(Origin, Actor->GetActorLocation()) <= RadiusSq)
				{
					Ref.TargetObject = Actor;
					Ref.ItemIndex = -1;
					OutBlocks.AddUnique(Ref);
				}
			}
		}
	}
}

FVector UBlockManagerSubsystem::GetBlockLocation(const FBlockReference& Ref)
{
	if (!Ref.IsValid()) return FVector::ZeroVector;

	// A. HISM 처리
	if (Ref.ItemIndex >= 0 && Ref.TargetComponent.IsValid())
	{
		if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(Ref.TargetComponent.Get()))
		{
			FTransform Trans;
			HISM->GetInstanceTransform(Ref.ItemIndex, Trans, true);
			return Trans.GetLocation();
		}
	}
	// B. Actor 처리
	else if (AActor* Actor = Cast<AActor>(Ref.TargetObject.Get()))
	{
		return Actor->GetActorLocation();
	}

	return FVector::ZeroVector;
}

void UBlockManagerSubsystem::HighlightBlock(const FBlockReference& BlockRef, const FGameplayTag& Tag)
{
	if (!BlockRef.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("HighlightBlock: Invalid Block Reference"));
	}

	FBlockCPDInfo CPDInfo = CachedBlockConfig->GetHighlightInfoByTag(Tag);

	if (BlockRef.ItemIndex >= 0) // HISM
	{
		if (AChunkBase* Chunk = Cast<AChunkBase>(BlockRef.TargetObject.Get()))
		{
			Chunk->HighlightHISMBlock(BlockRef.TargetComponent.Get(), BlockRef.ItemIndex, Tag);
		}
	}
	else // Actor
	{
		// Actor를 직접 캐스팅하는 대신, 약속된 인터페이스를 통해 메시지 전달
		if (IGameplayEventInterface* EventInterface = Cast<IGameplayEventInterface>(BlockRef.TargetObject.Get()))
		{
			FGameplayEventData Payload;
			Payload.EventTag = Tag;
			Payload.Instigator = nullptr;

			// BlockBase::HandleGameplayEvent가 호출됨 -> 내부에서 CPD 변경 로직 수행
			EventInterface->HandleGameplayEvent(Tag, Payload);
		}
	}
}

void UBlockManagerSubsystem::DestroyBlocksInRadius(const FVector& Origin, float Radius)
{
	if (!MapManager) {
		UE_LOG(LogTemp, Error, TEXT("DestroyBlocksInRadius: MapManager is not registered!"));
		return;
	}

	// 그리드 순회 로직 (이전에 작성한 로직과 동일)
	float GridSize = 100.0f;
	int32 RangeSteps = FMath::CeilToInt(Radius / GridSize);

	for (int32 x = -RangeSteps; x <= RangeSteps; x++)
	{
		for (int32 y = -RangeSteps; y <= RangeSteps; y++)
		{
			for (int32 z = -RangeSteps; z <= RangeSteps; z++)
			{
				FVector Offset(x * GridSize, y * GridSize, z * GridSize);
				if (Offset.SizeSquared() <= FMath::Square(Radius + (GridSize * 0.5f)))
				{
					FVector CheckLocation = Origin + Offset;
					if (AChunkBase* Chunk = MapManager->GetChunkAtLocation(CheckLocation))
					{
						Chunk->RemoveBlockAtWorldLocation(CheckLocation);
					}
				}
			}
		}
	}
}
