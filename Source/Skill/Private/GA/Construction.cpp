// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/Construction.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "BlockSystemInterface.h"
#include "PreviewTask.h"

void UConstruction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 1. 프리뷰 모드 진입: 태그 부착 (SkillBase 기능)
	AddGameplayTagToOwner(Tag_State_Preview);

	// 2. 프리뷰 태스크 시작 (매 프레임 하이라이트 갱신)
	PreviewTask = UPreviewTask::CreatePreviewTask(
		this,
		PreviewRange,
		Tag_Highlight_Range,
		Tag_Highlight_Cursor
	);

	// Task가 준비되었으면 활성화
	if (PreviewTask)
	{
		PreviewTask->ReadyForActivation();
	}

	// 3. 입력 대기
	// Confirm (좌클릭)
	UAbilityTask_WaitGameplayEvent* WaitConfirm = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		Tag_Event_Confirm,
		nullptr, // GA의 소유자가 아닌 다른 특정 액터의 ASC에서 발생하는 Event를 기다릴 때 사용
		false,   // 이벤트가 감지되면 Task 종료 (true) or 계속 대기 (false)
		false    // 하위 Gameplay Tag는 무시 (true) or 포함 (false)
	);
	WaitConfirm->EventReceived.AddDynamic(this, &UConstruction::OnConfirmEventReceived);
	WaitConfirm->ReadyForActivation();
}

void UConstruction::OnConfirmEventReceived(FGameplayEventData Payload)
{
	// 프리뷰 상태 종료
	// 현재는 애니메이션이 없으므로 일단 보류
	// RemoveGameplayTagFromOwner(Tag_State_Preview);

	// 소환 로직 수행
	UWorld* World = GetWorld();
	APlayerController* PC = GetActorInfo().PlayerController.Get();

	if (BlockSystem && PC)
	{
		FBlockReference TargetBlock;
		// 마우스 커서 아래 블록 가져오기
		if (BlockSystem->GetBlockUnderCursor(PC, TargetBlock))
		{
			// Task를 통해 프리뷰 블록 내부인지 검증
			if (PreviewTask->IsBlockInPreview(TargetBlock))
			{
				FVector TargetLoc = BlockSystem->GetBlockLocation(TargetBlock);
				FVector SpawnLoc = TargetLoc + FVector(0, 0, BlockSystem->GetGridSize());

				BlockSystem->SpawnBlockByTag(BlockTagToSpawn, SpawnLoc, FRotator::ZeroRotator, true);
			}
			else
			{
				// 범위 밖 클릭 등 무시
				UE_LOG(LogTemp, Log, TEXT("Click ignored: Block is out of range."));
			}
		}
	}
	else UE_LOG(LogTemp, Warning, TEXT("GA_Construction: Missing BlockSystem or PlayerController"));

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UConstruction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 종료 처리: 태그 제거
	RemoveGameplayTagFromOwner(Tag_State_Preview);

	// 태스크 정리
	if (PreviewTask)
	{
		PreviewTask->EndTask();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}