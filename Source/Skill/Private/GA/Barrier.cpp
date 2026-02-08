// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/Barrier.h"
#include "PreviewTask.h"
#include "BlockSystemInterface.h"
#include "BlockPreviewInterface.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"

UBarrier::UBarrier()
{
	CurrentPreviewRotation = FRotator::ZeroRotator;
}

void UBarrier::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 프리뷰 모드 진입: 태그 부착
	AddGameplayTagToOwner(Tag_Player_State_Preview);
	AddAbilityTag(Tag_Skill_State_Preview);

	// 초기 회전값 설정 (캐릭터가 바라보는 방향 기준 90도 스냅)
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		float Yaw = Avatar->GetActorRotation().Yaw;
		// 90도 단위 스냅 (0, 90, 180, 270)
		float SnappedYaw = FMath::GridSnap(Yaw, 90.0f);
		CurrentPreviewRotation = FRotator(0, SnappedYaw, 0);
	}

	// 프리뷰 태스크 시작
	PreviewTask = UPreviewTask::CreatePreviewTask(
		this,
		PreviewRange,
		Tag_Highlight_Range,
		Tag_Highlight_Cursor,
		nullptr,
		PreviewActorClass 
	);

	// Task 활성화
	if (PreviewTask)
	{
		PreviewTask->ReadyForActivation();

		// [중요] 초기 회전 적용
		// PreviewTask가 생성한 Actor에 접근할 수 있어야 함 (Getter 필요)
		if (AActor* SpawnedActor = PreviewTask->GetSpawnedPreviewActor())
		{
			SpawnedActor->SetActorRotation(CurrentPreviewRotation);
		}
	}

	// 입력 대기 (Confirm: 좌클릭)
	UAbilityTask_WaitGameplayEvent* WaitConfirm = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		Tag_Event_Confirm,
		nullptr,
		false,
		false
	);
	WaitConfirm->EventReceived.AddDynamic(this, &UBarrier::OnConfirmEventReceived);
	WaitConfirm->ReadyForActivation();

	// 입력 대기 (Rotation: 마우스 휠)
	UAbilityTask_WaitGameplayEvent* WaitWheel = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		Tag_Event_Rotation,
		nullptr,
		false,
		false
	);
	WaitWheel->EventReceived.AddDynamic(this, &UBarrier::OnMouseWheelEventReceived);
	WaitWheel->ReadyForActivation();
}

void UBarrier::OnMouseWheelEventReceived(FGameplayEventData Payload)
{
	if (!PreviewTask) return;

	// 90도 회전
	CurrentPreviewRotation.Yaw += 90.0f;

	// 정규화 (-180 ~ 180)
	CurrentPreviewRotation.Normalize();

	// 프리뷰 액터에 회전 적용
	if (AActor* SpawnedActor = PreviewTask->GetSpawnedPreviewActor())
	{
		SpawnedActor->SetActorRotation(CurrentPreviewRotation);
	}
}

void UBarrier::OnConfirmEventReceived(FGameplayEventData Payload)
{
	if (!PreviewTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	AActor* SpawnedActor = PreviewTask->GetSpawnedPreviewActor();
	IBlockPreviewInterface* PreviewActor;
	if (!(PreviewActor = Cast<IBlockPreviewInterface>(SpawnedActor)))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 1. 프리뷰 액터에게 소환할 수 있는 블록 위치 목록 요청
	TArray<FTransform> SpawnTransforms = PreviewActor->GetValidSpawnData();

	// 2. 유효한 데이터가 있다면 실제 소환 진행
	if (SpawnTransforms.Num() > 0)
	{
		if (BlockSystem)
		{
			TArray<FBlockSpawnRequest> FinalRequests;
			for (auto& SpawnTransform : SpawnTransforms)
			{
				FBlockSpawnRequest Req(BlockTagToSpawn, SpawnTransform.GetLocation(), true);
				FinalRequests.Add(Req);
			}

			// 일괄 소환 요청
			BlockSystem->SpawnBlocksBatch(FinalRequests);
		}
		else UE_LOG(LogTemp, Warning, TEXT("Barrier: BlockSystem is null. Cannot spawn blocks."));

		// 성공 종료
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	else
	{
		// 설치 불가능한 위치를 클릭함
		UE_LOG(LogTemp, Log, TEXT("Barrier: Cannot build here. Invalid location."));
	}
}

void UBarrier::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 종료 처리: 태그 제거
	RemoveGameplayTagFromOwner(Tag_Player_State_Preview);
	RemoveAbilityTag(Tag_Skill_State_Preview);

	if (PreviewTask)
	{
		PreviewTask->EndTask();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}