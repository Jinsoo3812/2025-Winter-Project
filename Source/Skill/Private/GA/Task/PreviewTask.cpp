// Fill out your copyright notice in the Description page of Project Settings.


#include "PreviewTask.h"
#include "SkillBase.h"
#include "BlockSystemInterface.h"
#include "BlockGameplayTags.h"
#include "CollisionChannels.h"
#include "Engine/OverlapResult.h"

UPreviewTask* UPreviewTask::CreatePreviewTask(USkillBase* OwningAbility, FSkillPreviewRange InRange, FGameplayTag InPreviewTag, FGameplayTag InCursorTag)
{
	UPreviewTask* NewTask = NewAbilityTask<UPreviewTask>(OwningAbility);
	NewTask->Range = InRange;
	NewTask->PreviewTag = InPreviewTag;
	NewTask->CursorTag = InCursorTag;
	NewTask->bTickingTask = true;
	return NewTask;
}

void UPreviewTask::Activate()
{
	Super::Activate();

	BlockSystem = Cast<USkillBase>(Ability)->GetBlockSystem();
}

void UPreviewTask::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!BlockSystem || !Ability || !Ability->GetCurrentActorInfo()->PlayerController.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PreviewTask: Missing BlockSystem or Ability or PlayerController"));
		return;
	}

	// 1. 이전 프레임 하이라이트 제거
	for (const FBlockReference& Ref : PreviousHighlights)
	{
		BlockSystem->HighlightBlock(Ref, TAG_Block_Highlight_None);
	}
	PreviousHighlights.Reset();


	// Range 세팅
	FCollisionShape CheckShape;
	AActor* Avatar = Ability->GetCurrentActorInfo()->AvatarActor.Get();
	if (!Avatar) {
		UE_LOG(LogTemp, Warning, TEXT("PreviewTask: AvatarActor is null"));
		return;
	}
	FVector Origin = Ability->GetCurrentActorInfo()->AvatarActor->GetActorLocation();
	FQuat Rotation = FQuat::Identity;

	// 오프셋 적용 (캐릭터가 바라보는 방향 기준)
	FVector Center = Origin + Avatar->GetActorRotation().RotateVector(Range.RelativeOffset);

	if (Range.ShapeType == EPreviewShapeType::Box)
	{
		CheckShape = FCollisionShape::MakeBox(Range.Dimensions);
		Rotation = Avatar->GetActorRotation().Quaternion(); // 박스는 회전 필요
	}
	else if (Range.ShapeType == EPreviewShapeType::Cylinder)
	{
		// 캡슐 생성 (Radius: X, HalfHeight: Z)
		CheckShape = FCollisionShape::MakeCapsule(Range.Dimensions.X, Range.Dimensions.Z);
		Rotation = FQuat::Identity; // 캡슐은 기본적으로 Z축으로 서 있음
	}

	// 물리 충돌 감지 (공통 로직)
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Block);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	UWorld* World = GetWorld();
	if (!World) {
		UE_LOG(LogTemp, Warning, TEXT("PreviewTask: World is null"));
		return;
	}
	World->OverlapMultiByObjectType(Overlaps, Center, Rotation, ObjectParams, CheckShape, QueryParams);

	// 변환 및 하이라이트
	TArray<FBlockReference> TargetBlocks;
	BlockSystem->GetBlocksFromOverlaps(Overlaps, TargetBlocks);

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
		PreviousHighlights.Add(Ref);
	}


	// 3. 마우스 커서 아래 블록 감지 (Cursor Highlight)
	APlayerController* PC = Ability->GetCurrentActorInfo()->PlayerController.Get();
	FBlockReference CursorBlock;

	// Subsystem 기능 활용: 마우스 아래 블록 조회 (User가 추가 요청한 함수)
	if (BlockSystem->GetBlockUnderCursor(PC, CursorBlock))
	{
		if (PreviousHighlights.Contains(CursorBlock))
		{
			// 커서 타겟 하이라이트
			BlockSystem->HighlightBlock(CursorBlock, CursorTag);
		}
	}
}

void UPreviewTask::OnDestroy(bool bInOwnerFinished)
{
	// 태스크 종료 시(스킬 취소/완료) 모든 하이라이트 끄기
	if (BlockSystem)
	{
		for (const FBlockReference& Ref : PreviousHighlights)
		{
			BlockSystem->HighlightBlock(Ref, TAG_Block_Highlight_None);
		}
	}
	PreviousHighlights.Empty();

	Super::OnDestroy(bInOwnerFinished);
}

bool UPreviewTask::IsBlockInPreview(const FBlockReference& BlockRef) const
{
	return PreviousHighlights.Contains(BlockRef);
}