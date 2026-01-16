// Fill out your copyright notice in the Description page of Project Settings.


#include "Block/BlockManagerSubsystem.h"
#include "Block/BlockBase.h"
#include "CollisionChannels.h"
#include "Block/BlockSettings.h"

void UBlockManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    IBlockSpawnInterface::RegisterSpawner(GetWorld(), this);

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
    UDA_BlockConfig* BlockConfig = Settings->BlockConfigAsset.LoadSynchronous();

    if (BlockConfig)
    {
        // 맵 데이터 캐싱
        BlockClassMap = BlockConfig->BlockClassMap;
        UE_LOG(LogTemp, Log, TEXT("BlockManagerSubsystem: Successfully loaded block config from Project Settings."));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("BlockManagerSubsystem: Failed to load BlockConfig asset."));
    }
}

void UBlockManagerSubsystem::Deinitialize()
{
    // [중요] 시스템 종료 시 반드시 등록 해제 (Dangling Pointer 방지)
    IBlockSpawnInterface::UnregisterSpawner(GetWorld());

    Super::Deinitialize();
}

AActor* UBlockManagerSubsystem::SpawnBlockByTag(FGameplayTag BlockTypeTag, FVector Location, FRotator Rotation, bool bEnableGravity)
{
    // 태그에 맞는 블록 클래스 찾기
    TSubclassOf<ABlockBase>* FoundClass = BlockClassMap.Find(BlockTypeTag);

    // 유효성 검사: 클래스를 찾지 못한 경우
    if (!FoundClass || !(*FoundClass))
    {
        UE_LOG(LogTemp, Warning, TEXT("BlockManagerSubsystem: No block class found for tag %s"), *BlockTypeTag.ToString());
        return nullptr;
    }
    // 클래스는 찾았으나 내부 포인터가 유효하지 않은 경우
    else if (!FoundClass->Get())
    {
        UE_LOG(LogTemp, Error, TEXT("BlockManagerSubsystem: Found class is invalid for tag %s"), *BlockTypeTag.ToString());
        return nullptr;
    }

    // CDO(Class Default Object)를 가져와서 생성될 블록의 기본 GridSize를 확인
    ABlockBase* CDO = FoundClass->Get()->GetDefaultObject<ABlockBase>();
    float GridSize = CDO ? CDO->GetGridSize() : 100.0f;

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

    ABlockBase* NewBlock = GetWorld()->SpawnActor<ABlockBase>(*FoundClass, Location, Rotation, SpawnParams);

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

        return NewBlock;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("BlockManagerSubsystem: Failed to spawn block at %s"), *Location.ToString());
        return nullptr;
    }
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

	FCollisionQueryParams QueryParams;

	// 충돌한 블록들을 반환할 것이 아니므로 OverlapMulti 대신 OverlapAny 사용
	return World->OverlapAnyTestByObjectType(CheckLocation, FQuat::Identity, ObjectQueryParams, CheckShape, QueryParams);
}