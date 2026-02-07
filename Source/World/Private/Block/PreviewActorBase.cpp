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

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(this, &APreviewActorBase::RebuildInstances);
    }
}

void APreviewActorBase::RebuildInstances()
{
    if (!HISMComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("PreviewActorBase: HISMComponent is null in RebuildInstances"));
		return;
    }
    /*
    // [해결책] 컴포넌트가 꺼져있다면 강제로 켭니다.
    if (!HISMComponent->IsActive())
    {
        HISMComponent->Activate(true); // bReset = true
        HISMComponent->SetComponentTickEnabled(true); // 틱도 강제로 켭니다.
        UE_LOG(LogTemp, Warning, TEXT("DeepDebug: HISM was Inactive! Forcing Activation..."));
    }
    */

    /*
    UStaticMesh* CurrentMesh = HISMComponent->GetStaticMesh();
    if (CurrentMesh)
    {
        // 원본 메시의 바운드 박스 크기를 확인합니다.
        FBoxSphereBounds MeshBounds = CurrentMesh->GetBounds();
        UE_LOG(LogTemp, Warning, TEXT("DeepDebug: [Mesh Origin Check] Name: %s | Radius: %f | Extent: %s"),
            *CurrentMesh->GetName(),
            MeshBounds.SphereRadius,
            *MeshBounds.BoxExtent.ToString());
    }
    else
    {
        // 메시가 있다면 이름 출력 (디버깅용)
        UE_LOG(LogTemp, Log, TEXT("[RebuildInstances] Using Mesh: %s"), *CurrentMesh->GetName());
    }
    */

    HISMComponent->ClearInstances();
    ValidInstanceIndices.Reset();

    TArray<FTransform> InstanceTransforms;
    InstanceTransforms.Reserve(RelativeBlockOffsets.Num());

    for (const FVector& Offset : RelativeBlockOffsets)
    {
        // 스케일 1.0 명시 필수
        InstanceTransforms.Emplace(FRotator::ZeroRotator, Offset, FVector::OneVector);
    }

    // 2. 일괄 추가 (여기가 핵심)
    // bShouldReturnIndices: false (인덱스 반환 불필요 시 성능 이득)
    HISMComponent->AddInstances(InstanceTransforms, false);

    // HISMComponent->BuildTreeIfOutdated(false, true);

    // 그 후 바운드와 렌더 상태를 갱신
    // HISMComponent->UpdateBounds();

    /*
    // ================= [Low-Level Debug Log] =================
    if (HISMComponent)
    {
        // 1. 월드 등록 여부 (이게 false면 렌더링 절대 불가)
        bool bIsRegistered = HISMComponent->IsRegistered();

        // 2. 렌더 상태 생성 여부 (SceneProxy가 만들어졌는지)
        bool bRenderState = HISMComponent->IsRenderStateCreated();

        // 3. 물리 상태 생성 여부 (PhysicsState)
        bool bPhysicsState = HISMComponent->IsPhysicsStateCreated();

        // 4. 컴포넌트가 활성화 상태인지
        bool bIsActive = HISMComponent->IsActive();

        FBoxSphereBounds Bounds = HISMComponent->Bounds;

        UE_LOG(LogTemp, Error, TEXT("DeepDebug: [Status Check] Registered: %s | RenderState: %s | Active: %s | Extent: %s"),
            bIsRegistered ? TEXT("YES") : TEXT("NO"),
            bRenderState ? TEXT("YES") : TEXT("NO"),
            bIsActive ? TEXT("YES") : TEXT("NO"),
            *Bounds.BoxExtent.ToString());
    }
    // =========================================================
    */

    // HISMComponent->MarkRenderStateDirty();

    /*
    if (HISMComponent->IsVisible())
    {
        HISMComponent->SetVisibility(false);
        HISMComponent->SetVisibility(true);
    }

    // ================= [로그 추가 시작] =================
    // HISM이 인스턴스를 다 넣은 직후, 렌더 상태와 바운드 크기를 확인합니다.
    if (HISMComponent)
    {
        FBoxSphereBounds Bounds = HISMComponent->Bounds;
        bool bRenderState = HISMComponent->IsRenderStateCreated();
        int32 Count = HISMComponent->GetInstanceCount();

        // Extent가 (0,0,0)이라면 엔진은 이 물체의 크기를 0으로 보고 있다는 뜻입니다.
        UE_LOG(LogTemp, Warning, TEXT("DeepDebug: [RebuildInstances] Count: %d | RenderStateCreated: %s | Extent: %s | Origin: %s"),
            Count,
            bRenderState ? TEXT("TRUE") : TEXT("FALSE"),
            *Bounds.BoxExtent.ToString(),
            *Bounds.Origin.ToString());
    }
    // ================= [로그 추가 끝] =================
    */
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