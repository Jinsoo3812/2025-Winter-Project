// Fill out your copyright notice in the Description page of Project Settings.


#include "PreviewActorBase.h"
#include "BlockSettings.h"
#include "BlockSystemInterface.h"
#include "BlockConfig.h"
#include "BlockGameplayTags.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

// Sets default values
APreviewActorBase::APreviewActorBase()
{
	PrimaryActorTick.bCanEverTick = false;

    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    RootComponent = RootScene;

    HISMComponent = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HISMComponent"));
    HISMComponent->SetupAttachment(RootComponent);

    // 프리뷰는 물리 충돌이 없어야 함
    HISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HISMComponent->SetGenerateOverlapEvents(false);

    // CPD 사용 설정
    HISMComponent->NumCustomDataFloats = 8;
}

void APreviewActorBase::BeginPlay()
{
    Super::BeginPlay();

    if (UWorld* World = GetWorld())
    {
        if (!BlockSystem) BlockSystem = IBlockSystemInterface::Get(World);
    }
	else UE_LOG(LogTemp, Error, TEXT("PreviewActorBase: World is null in BeginPlay"));

    if (!BlockConfig)
    {
        if (const UBlockSettings* Settings = GetDefault<UBlockSettings>())
        {
            if (!Settings->BlockConfigAsset.IsNull())
            {
                // 디스크에서 에셋을 찾아 메모리에 올림
                // 로딩이 완료될 때까지 이곳에서 실행흐름이 멈춤
                BlockConfig = Settings->BlockConfigAsset.LoadSynchronous();
            }
			else UE_LOG(LogTemp, Error, TEXT("PreviewActorBase: BlockConfigAsset is null in BeginPlay"));
		}
		else UE_LOG(LogTemp, Error, TEXT("PreviewActorBase: Failed to get BlockSettings in BeginPlay"));
    }

    // 인스턴스 초기 배치
    RebuildInstances();
}

void APreviewActorBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    RebuildInstances();
}

void APreviewActorBase::RebuildInstances()
{
    if (!HISMComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("PreviewActorBase: HISMComponent is null in RebuildInstances"));
		return;
    }

    HISMComponent->ClearInstances();
    ValidInstanceIndices.Reset();

    // 설정된 오프셋만큼 인스턴스 생성
    for (const FVector& Offset : RelativeBlockOffsets)
    {
        // 로컬 좌표로 배치
        FTransform Trans(FRotator::ZeroRotator, Offset);
        HISMComponent->AddInstance(Trans);
    }
}

bool APreviewActorBase::UpdatePreviewState_Implementation(FVector TargetLocation)
{
    SetActorLocation(TargetLocation);

    // CPD 정보 찾기 
    const FBlockCPDInfo* ValidCPD = BlockConfig->HighlightSettings.Find(TAG_Block_Highlight_None);
    const FBlockCPDInfo* InvalidCPD = BlockConfig->HighlightSettings.Find(TAG_Block_Highlight_Invalid);

    float ValidValue = ValidCPD ? ValidCPD->CPDValue : 0.0f;
    float InvalidValue = InvalidCPD ? InvalidCPD->CPDValue : 1.0f;
    int32 CPDIndex = ValidCPD ? ValidCPD->CPDIndex : 0;

    // 인스턴스 순회 및 검사
    bool bAnchorValid = !BlockSystem->IsLocationOccupied(TargetLocation, BlockConfig->GridSize);

    for (int32 i = 0; i < HISMComponent->GetInstanceCount(); ++i)
    {
        FTransform InstanceTrans;
        HISMComponent->GetInstanceTransform(i, InstanceTrans, true);
        FVector Loc = InstanceTrans.GetLocation();

        bool bOccupied = BlockSystem->IsLocationOccupied(Loc, BlockConfig->GridSize);
        
		// 커서(앵커) 위치가 유효하지 않으면 모든 인스턴스를 Invalid로 표시하기 위함
        bool bInstallable = bAnchorValid && !bOccupied;

        // CPD 값 적용
        if (bInstallable)
        {
            HISMComponent->SetCustomDataValue(i, CPDIndex, ValidValue, true);
            ValidInstanceIndices.Add(i);
        }
        else HISMComponent->SetCustomDataValue(i, CPDIndex, InvalidValue, true);
    }

    return bAnchorValid;
}

TArray<FPreviewSpawnData> APreviewActorBase::GetValidSpawnData_Implementation()
{
    TArray<FPreviewSpawnData> SpawnDataList;

    // UpdatePreviewState에서 계산해둔 '유효한 인덱스'들만 순회
    for (int32 Idx : ValidInstanceIndices)
    {
        FTransform InstanceTrans;
        // 월드 좌표 트랜스폼 가져오기
        HISMComponent->GetInstanceTransform(Idx, InstanceTrans, true);

        FPreviewSpawnData NewData;
        NewData.BlockTag = TargetBlockTag;
        NewData.SpawnTransform = InstanceTrans;

        SpawnDataList.Add(NewData);
    }

    return SpawnDataList;
}