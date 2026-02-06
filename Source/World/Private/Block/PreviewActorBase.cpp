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
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
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
}

bool APreviewActorBase::UpdatePreviewState(FVector TargetLocation)
{
    SetActorLocation(TargetLocation); // 회전 처리는 별도 로직 필요

    // 2. CPD 정보 찾기 (예: Valid / Invalid 태그)
    // 태그 이름은 예시입니다. BlockGameplayTags.h 등에 정의되어 있어야 함
    const FBlockCPDInfo* ValidCPD = BlockConfig->HighlightSettings.Find(TAG_Block_Highlight_None);
    const FBlockCPDInfo* InvalidCPD = BlockConfig->HighlightSettings.Find(TAG_Block_Highlight_Invalid);

    // 3. 인스턴스 순회 및 검사
    bool bAnchorValid = !BlockSystem->IsLocationOccupied(TargetLocation, BlockConfig->GridSize);

    for (int32 i = 0; i < HISMComponent->GetInstanceCount(); ++i)
    {
        FTransform InstanceTrans;
        HISMComponent->GetInstanceTransform(i, InstanceTrans, true);
        FVector Loc = InstanceTrans.GetLocation();

        bool bOccupied = BlockSystem->IsLocationOccupied(Loc, BlockConfig->GridSize);
        
		// 커서(앵커) 위치가 유효하지 않으면 모든 인스턴스를 Invalid로 표시하기 위함
        bool bVisualValid = bAnchorValid && !bOccupied;

        // CPD 값 적용
        const FBlockCPDInfo* TargetCPD = bVisualValid ? InvalidCPD : ValidCPD;
        if (TargetCPD)
        {
            HISMComponent->SetCustomDataValue(i, TargetCPD->CPDIndex, TargetCPD->CPDValue, true);
        }
		else UE_LOG(LogTemp, Warning, TEXT("PreviewActorBase: Missing CPD info for %s."), bOccupied ? TEXT("Invalid") : TEXT("Valid"));
    }

    return bAnchorValid;
}