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

	// 현재 월드와 서브시스템 자신을 매핑
	IBlockSystemInterface::RegisterSystem(GetWorld(), this);

	// 개발자 설정(Project Settings)에서 설정 객체 가져오기
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

	// BlockConfig 데이터 에셋을 동기 로드 및 캐싱
	CachedBlockConfig = Settings->BlockConfigAsset.LoadSynchronous();

	if(!CachedBlockConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("BlockManagerSubsystem: Failed to load BlockConfig from Asset."));
		return;
	}
}

void UBlockManagerSubsystem::Deinitialize()
{
	// 시스템 종료 시 반드시 등록 해제
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
					// 청크가 유효하지 않으므로 넘어감
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

	// GameplayTag로 블록 클래스 찾기
	TSubclassOf<AActor> FoundActorClass = CachedBlockConfig->GetBlockDef(BlockTypeTag)->ActorClass;

	if (!FoundActorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockManagerSubsystem: No block class found for tag %s"), *BlockTypeTag.ToString());
		return nullptr;
	}

	// GridSize 가져오기
	float GridSize = 100.0f;
	if (MapManager) {
		GridSize = MapManager->GridSize;
	}

	// 위치 점유 확인
	if (IsLocationOccupied(Location, GridSize))
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockManagerSubsystem: Location %s is occupied"), *Location.ToString());
		return nullptr;
	}

	// 소환
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; // 점유 확인했으므로 항상 소환

	AActor* NewActor = GetWorld()->SpawnActor<AActor>(FoundActorClass, Location, Rotation, SpawnParams);
	ABlockBase* NewBlock = Cast<ABlockBase>(NewActor);

	// 생성 후 설정 (중력, 위치 보정 등)
	if (NewBlock)
	{
		// 위치를 명확하게 다시 설정 (SpawnActor 시 미세한 오차 방지)
		NewBlock->SetActorLocation(Location);

		// 중력 설정
		if (bEnableGravity)
		{
			NewBlock->SetCanFall(true);
			NewBlock->SetActorTickEnabled(true);
		}
		else
		{
			NewBlock->SetCanFall(false);
			NewBlock->SetActorTickEnabled(false);
		}

		
		// 청크 시스템과 동기화
		if (MapManager && CachedBlockConfig)
		{
			if (AChunkBase* TargetChunk = MapManager->GetChunkAtLocation(Location))
			{
				// 태그를 이용해 정확한 EBlockType 찾기
				EBlockType TargetType = CachedBlockConfig->GetBlockDef(BlockTypeTag)->Type;

				// Config에 없는 Tag라면 Destructible로 기본 설정
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
				EBlockType FoundType = CachedBlockConfig->GetBlockDef(Req.BlockTag)->Type;
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

FVector UBlockManagerSubsystem::GetBlockLocation(const FBlockReference& Ref)
{
	if (!Ref.IsValid()) return FVector::ZeroVector;

	// A. HISM 처리
	if (Ref.ItemIndex >= 0 && Ref.TargetComponent.IsValid())
	{
		if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(Ref.TargetComponent.Get()))
		{
			FTransform Trans;
			// 월드 좌표로 반환
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
		return;
	}

	if (BlockRef.ItemIndex >= 0) // HISM
	{
		if (AChunkBase* Chunk = Cast<AChunkBase>(BlockRef.TargetObject.Get()))
		{
			Chunk->HighlightHISMBlock(BlockRef.TargetComponent.Get(), BlockRef.ItemIndex, Tag);
		}
	}
	else // Actor
	{
		if (IGameplayEventInterface* EventInterface = Cast<IGameplayEventInterface>(BlockRef.TargetObject.Get()))
		{
			FGameplayEventData Payload;
			Payload.EventTag = Tag;
			Payload.Instigator = nullptr;

			EventInterface->HandleGameplayEvent(Tag, Payload);
		}
	}
}

bool UBlockManagerSubsystem::GetBlockUnderCursor(const APlayerController* PlayerController, FBlockReference& OutBlockRef) {
	if (!PlayerController) return false;

	FHitResult HitResult;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;

	// ECC_Block 타입만 검사 대상으로 추가
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Block));

	bool bHit = PlayerController->GetHitResultUnderCursorForObjects(
		ObjectTypes,
		true, // bTraceComplex (HISM 인덱스 얻으려면 필수)
		HitResult
	);

	if (bHit)
	{
		return GetBlockFromHitResult(HitResult, OutBlockRef);
	}

	return false;
}

bool UBlockManagerSubsystem::GetBlockFromHitResult(const FHitResult& HitResult, FBlockReference& OutBlockRef)
{
	AActor* HitActor = HitResult.GetActor();
	UPrimitiveComponent* HitComp = HitResult.GetComponent();

	if (!HitActor || !HitComp) return false;

	// Case A: HISM
	if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(HitComp))
	{
		// ChunkBase인지 확인
		if (AChunkBase* Chunk = Cast<AChunkBase>(HitActor))
		{
			// HISM은 HitResult.Item에 인스턴스 인덱스가 들어옴
			if (HitResult.Item != INDEX_NONE)
			{
				OutBlockRef.TargetObject = Chunk;
				OutBlockRef.TargetComponent = HISM;
				OutBlockRef.ItemIndex = HitResult.Item;
				return true;
			}
			else UE_LOG(LogTemp, Warning, TEXT("GetBlockFromHitResult: HitResult.Item is INDEX_NONE for HISM component."));
		}
		else UE_LOG(LogTemp, Warning, TEXT("GetBlockFromHitResult: Hit Actor is not a ChunkBase for HISM component."));
	}
	// Case B: Actor
	else if (ABlockBase* BlockActor = Cast<ABlockBase>(HitActor))
	{
		OutBlockRef.TargetObject = BlockActor;
		OutBlockRef.TargetComponent = nullptr;
		OutBlockRef.ItemIndex = -1;
		return true;
	}

	return false;
}

void UBlockManagerSubsystem::GetBlocksFromOverlaps(const TArray<FOverlapResult>& Overlaps, TArray<FBlockReference>& OutBlocks)
{
	OutBlocks.Reset();

	for (const FOverlapResult& Result : Overlaps)
	{
		// Case A: HISM
		if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(Result.GetComponent()))
		{
			if (AChunkBase* Chunk = Cast<AChunkBase>(Result.GetActor()))
			{
				FBlockReference Ref;
				Ref.TargetObject = Chunk;
				Ref.TargetComponent = HISM;
				Ref.ItemIndex = Result.ItemIndex;

				// 중복 방지 (HISM은 여러 번 겹칠 수 있음)
				OutBlocks.AddUnique(Ref);
			}
		}
		// Case B: Actor
		else if (AActor* Actor = Result.GetActor())
		{
			// 인터페이스 구현 여부 확인
			if (Actor->Implements<UGameplayEventInterface>())
			{
				FBlockReference Ref;
				Ref.TargetObject = Actor;
				Ref.ItemIndex = -1;
				OutBlocks.AddUnique(Ref);
			}
		}
	}
}