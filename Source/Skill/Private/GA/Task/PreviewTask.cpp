// Fill out your copyright notice in the Description page of Project Settings.


#include "PreviewTask.h"
#include "SkillBase.h"
#include "BlockSystemInterface.h"
#include "BlockGameplayTags.h"
#include "CollisionChannels.h"
#include "Engine/OverlapResult.h"
#include "BlockPreviewInterface.h"

UPreviewTask* UPreviewTask::CreatePreviewTask(
	USkillBase* OwningAbility,
	FSkillPreviewRange InRange,
	FGameplayTag InPreviewTag,
	FGameplayTag InCursorTag,
	TSubclassOf<AActor> InVisualizerClass,
	TSubclassOf<AActor> InGhostBlockClass
)
{
	UPreviewTask* NewTask = NewAbilityTask<UPreviewTask>(OwningAbility);
	NewTask->Range = InRange;
	NewTask->PreviewTag = InPreviewTag;
	NewTask->CursorTag = InCursorTag;
	NewTask->VisualizerClass = InVisualizerClass;
	NewTask->GhostBlockClass = InGhostBlockClass;

	NewTask->bTickingTask = true;
	return NewTask;
}

void UPreviewTask::Activate()
{
	Super::Activate();

	BlockSystem = Cast<USkillBase>(Ability)->GetBlockSystem();

	if (!GetWorld()) {
		UE_LOG(LogTemp, Warning, TEXT("PreviewTask: Missing World"));
		return;
	}

	if (VisualizerClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// 스폰만 해둠 (위치/크기는 Tick에서 갱신)
		SpawnedVisualizer = GetWorld()->SpawnActor<AActor>(VisualizerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

		if (SpawnedVisualizer)
		{
			// 충돌 끄기
			SpawnedVisualizer->SetActorEnableCollision(false);
		}
		else UE_LOG(LogTemp, Warning, TEXT("PreviewTask: Failed to spawn Visualizer Actor"));
	}

	if(GhostBlockClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedGhostBlock = GetWorld()->SpawnActor<AActor>(GhostBlockClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (SpawnedGhostBlock)
		{
			SpawnedGhostBlock->SetActorEnableCollision(false);
			SpawnedGhostBlock->SetActorHiddenInGame(true);
		}
		else UE_LOG(LogTemp, Warning, TEXT("PreviewTask: Failed to spawn GhostBlock Actor"));
	}
}

void UPreviewTask::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!BlockSystem || !Ability || !Ability->GetCurrentActorInfo()->PlayerController.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PreviewTask: Missing BlockSystem or Ability or PlayerController"));
		return;
	}

	AActor* Avatar = Ability->GetCurrentActorInfo()->AvatarActor.Get();
	if (!Avatar) {
		UE_LOG(LogTemp, Warning, TEXT("PreviewTask: Missing Avatar Actor"));
		return;
	}

	// 1. 이전 프레임 하이라이트 제거
	for (const FBlockReference& Ref : HighlightedBlocks)
	{
		BlockSystem->HighlightBlock(Ref, TAG_Block_Highlight_None);
	}
	HighlightedBlocks.Reset();


	// 2. 범위 계산 및 시각화, 오버랩 수행
	TArray<FOverlapResult> Overlaps;
	UpdatePreviewShapeAndOverlap(Overlaps, Avatar);

	// 3. 오버랩 결과 중 BlockReference 추출
	TArray<FBlockReference> TargetBlocks;
	BlockSystem->GetBlocksFromOverlaps(Overlaps, TargetBlocks);

	// 4. 하이라이트 적용
	FVector Center = Avatar->GetActorLocation() + Avatar->GetActorRotation().RotateVector(Range.RelativeOffset);
	for (const FBlockReference& Ref : TargetBlocks)
	{
		// Capsule로 필터링 하였으므로, 높이 검사를 추가 하여 원기둥 검사가 되도록
		if (Range.ShapeType == EPreviewShapeType::Cylinder)
		{
			FVector BlockLoc = BlockSystem->GetBlockLocation(Ref);
			float DeltaZ = FMath::Abs(BlockLoc.Z - Center.Z);

			if (DeltaZ > Range.Dimensions.Z)
			{
				continue;
			}
		}
		BlockSystem->HighlightBlock(Ref, PreviewTag);
		HighlightedBlocks.Add(Ref);
	}


	// 5. 마우스 커서 아래 블록 로직
	bool bCursorHighlight = (CursorTag != FGameplayTag::EmptyTag);
	bool bShowGhostBlock = (SpawnedGhostBlock != nullptr);

	if (bCursorHighlight || bShowGhostBlock) {
		APlayerController* PC = Ability->GetCurrentActorInfo()->PlayerController.Get();
		FBlockReference CursorBlock;

		// Subsystem 기능 활용: 마우스 아래 블록 조회 (User가 추가 요청한 함수)
		if (BlockSystem->GetBlockUnderCursor(PC, CursorBlock))
		{
			if (HighlightedBlocks.Contains(CursorBlock))
			{
				// 커서 타겟 하이라이트
				if (bCursorHighlight) {
					BlockSystem->HighlightBlock(CursorBlock, CursorTag);
					CurrentCursorBlock = CursorBlock;
				}

				// 고스트 블록 시각화
				if (bShowGhostBlock)
				{
					FVector TargetLoc = BlockSystem->GetBlockLocation(CursorBlock);
					float GridSize = BlockSystem->GetGridSize();

					// 타겟 블록 위에 스냅
					FVector PreviewLoc = TargetLoc + FVector(0, 0, GridSize);

					// 인터페이스 구현 여부 확인
					if (SpawnedGhostBlock->Implements<UBlockPreviewInterface>())
					{
						// 액터를 보이게 설정
						SpawnedGhostBlock->SetActorHiddenInGame(false);

						// 인터페이스 호출: 액터 스스로 위치를 잡고, 유효성을 판단하여 색상을 바꿈
						bool bIsInstallable = IBlockPreviewInterface::Execute_UpdatePreviewState(SpawnedGhostBlock, PreviewLoc);
					}
					else
					{
						// 인터페이스가 없는 단순 액터인 경우 기존 방식대로 이동
						SpawnedGhostBlock->SetActorLocation(PreviewLoc);
						SpawnedGhostBlock->SetActorHiddenInGame(false);
					}
				}
			}
			else {
				CurrentCursorBlock.Reset();
				if (bShowGhostBlock) SpawnedGhostBlock->SetActorHiddenInGame(true);
			}
		}
	}
}

void UPreviewTask::UpdatePreviewShapeAndOverlap(TArray<FOverlapResult>& OutOverlaps, AActor* Avatar)
{
	if (!Avatar || !GetWorld()) return;


	APlayerController* PC = Ability->GetCurrentActorInfo()->PlayerController.Get();
	if (!PC) return;

	// 1. 마우스 위치 계산
	FVector Origin = Avatar->GetActorLocation();
	FVector MouseLocationOnPlane;

	FVector WorldLoc, WorldDir;
	// 마우스 화면 좌표 -> 월드 Ray 변환
	if (PC->DeprojectMousePositionToWorld(WorldLoc, WorldDir))
	{
		// FMath::RayPlaneIntersection : 엔진 기본 제공 함수 (Ray와 평면의 교차점)
		// 평면(Plane) 정의: (원점: 캐릭터 위치, 법선: 위쪽)
		FPlane GroundPlane(Origin, FVector::UpVector);

		MouseLocationOnPlane = FMath::RayPlaneIntersection(WorldLoc, WorldDir, GroundPlane);
	}
	else
	{
		MouseLocationOnPlane = Origin + Avatar->GetActorForwardVector() * 100.0f;
	}


	// 2. 방향 및 회전 계산

	FVector Direction = MouseLocationOnPlane - Origin;
	FRotator LookAtRotation = Direction.Rotation();

	FVector Center = Origin + LookAtRotation.RotateVector(Range.RelativeOffset);

	// 2. 모양에 따른 쉐이프 및 스케일 결정
	// 시각화용 BP 액터의 기본 메시는 100x100x100 사이즈라고 가정
	FQuat ShapeRotation = FQuat::Identity;
	FCollisionShape CheckShape;
	FVector NewScale = FVector::OneVector;

	if (Range.ShapeType == EPreviewShapeType::Box)
	{
		CheckShape = FCollisionShape::MakeBox(Range.Dimensions);

		// 박스는 마우스 방향을 바라봐야 함 (LookAtRotation 적용)
		ShapeRotation = LookAtRotation.Quaternion();

		// Box Dimensions는 Extent(반지름)이므로 * 2, 기본 크기 100으로 나눔
		NewScale = (Range.Dimensions * 2.0f) / 100.0f;
	}
	else if (Range.ShapeType == EPreviewShapeType::Cylinder)
	{
		CheckShape = FCollisionShape::MakeCapsule(Range.Dimensions.X, Range.Dimensions.Z);

		// 원기둥은 회전해도 모양이 같으므로 Identity여도 되지만, 
		// 만약 타원형 등을 고려한다면 ShapeRotation을 적용하는 것이 좋음.
		// 보통은 그냥 둠.
		ShapeRotation = FQuat::Identity;

		// X: 반지름, Z: 반높이
		float ScaleXY = (Range.Dimensions.X * 2.0f) / 100.0f;
		float ScaleZ = (Range.Dimensions.Z * 2.0f) / 100.0f;
		NewScale = FVector(ScaleXY, ScaleXY, ScaleZ);
	}

	// 3. 시각화 액터 동기화
	if (SpawnedVisualizer)
	{
		SpawnedVisualizer->SetActorLocationAndRotation(Center, ShapeRotation);
		SpawnedVisualizer->SetActorScale3D(NewScale);
	}

	// 4. 물리 충돌 감지
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Block);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	GetWorld()->OverlapMultiByObjectType(OutOverlaps, Center, ShapeRotation, ObjectParams, CheckShape, QueryParams);
}

void UPreviewTask::OnDestroy(bool bInOwnerFinished)
{
	// 태스크 종료 시(스킬 취소/완료) 모든 하이라이트 끄기
	if (BlockSystem)
	{
		for (const FBlockReference& Ref : HighlightedBlocks)
		{
			BlockSystem->HighlightBlock(Ref, TAG_Block_Highlight_None);
		}
	}
	HighlightedBlocks.Empty();

	// 시각화 액터 파괴
	if (SpawnedVisualizer)
	{
		SpawnedVisualizer->Destroy();
		SpawnedVisualizer = nullptr;
	}

	if(SpawnedGhostBlock)
	{
		SpawnedGhostBlock->Destroy();
		SpawnedGhostBlock = nullptr;
	}

	Super::OnDestroy(bInOwnerFinished);
}

bool UPreviewTask::IsBlockInPreview(const FBlockReference& BlockRef) const
{
	return HighlightedBlocks.Contains(BlockRef);
}