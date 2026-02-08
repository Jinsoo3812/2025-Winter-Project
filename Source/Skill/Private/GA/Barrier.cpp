// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/Barrier.h"
#include "PreviewTask.h"
#include "BlockSystemInterface.h"
#include "BlockPreviewInterface.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

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

	// 생성 프리뷰 & 발사 모드 분기
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && ASC->HasMatchingGameplayTag(Tag_Player_State_Active_Barrier)) {
		// 발사 후 즉시 반환
		Launch();
		return;
	}
	
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

void UBarrier::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
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

	// 발사 방향 결정
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		FVector PlayerLoc = Avatar->GetActorLocation();
		FVector TargetLoc = SpawnedActor->GetActorLocation();

		// 1. 시전자에서 목표 지점으로 향하는 벡터 계산
		FVector Dir = TargetLoc - PlayerLoc;
		Dir.Z = 0.0f; // 높이 차이 무시 (수평 방향만 고려)

		// 가장 지배적인 축(Dominant Axis)을 찾아 4방향으로 스냅
		// X축의 크기가 Y축보다 크면 앞/뒤, 아니면 좌/우
		if (FMath::Abs(Dir.X) > FMath::Abs(Dir.Y))
		{
			// X 성분의 부호(Sign)만 남김 (1 or -1)
			LaunchDirection = FVector(FMath::Sign(Dir.X), 0.0f, 0.0f);
		}
		else
		{
			// Y 성분의 부호(Sign)만 남김 (1 or -1)
			LaunchDirection = FVector(0.0f, FMath::Sign(Dir.Y), 0.0f);
		}
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

			// 콜백 델리게이트 생성 및 바인딩
			FOnBlockBatchSpawnComplete OnCompleteDelegate;
			OnCompleteDelegate.BindUObject(this, &UBarrier::OnBlocksSpawned);

			// 일괄 소환 요청
			BlockSystem->SpawnBlocksBatch(FinalRequests, OnCompleteDelegate);
		}
		else UE_LOG(LogTemp, Warning, TEXT("Barrier: BlockSystem is null. Cannot spawn blocks."));

		// 블록 스폰 비동기 작업 완료 후 EndAbility 호출
	}
	else
	{
		// 설치 불가능한 위치를 클릭함
		UE_LOG(LogTemp, Log, TEXT("Barrier: Cannot build here. Invalid location."));
	}
}

void UBarrier::OnBlocksSpawned(const TArray<TWeakObjectPtr<AActor>>& SpawnedBlocks)
{
	CurrentBarrierBlocks = SpawnedBlocks;

	if (CurrentBarrierBlocks.Num() > 0)
	{
		// 블록 생성 성공 시 시전자에게 상태 태그 부착
		AddGameplayTagToOwner(Tag_Player_State_Active_Barrier);
	}

	// 블록이 생성 확정된 후 안전하게 종료
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UBarrier::Launch()
{
	UE_LOG(LogTemp, Log, TEXT("Barrier: Launch Sequence Initiated."));

	if (CurrentBarrierBlocks.Num() > 0)
	{
		for (auto& BlockPtr : CurrentBarrierBlocks)
		{
			if (AActor* BlockActor = BlockPtr.Get())
			{
				UFunction* LaunchFunc = BlockActor->FindFunction(FName("Launch"));

				if (LaunchFunc)
				{
					// 파라미터 구조체 정의 (함수 인자와 순서/타입이 일치해야 함)
					struct FLaunchParams
					{
						FVector Direction;
					};

					FLaunchParams Params;
					Params.Direction = LaunchDirection;

					// 함수 실행
					BlockActor->ProcessEvent(LaunchFunc, &Params);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("BlockActor does not have Launch function!"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Barrier: Block is invalid or destroyed."));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Barrier: No blocks to launch."));
	}

	// 2. 상태 해제 및 종료
	RemoveGameplayTagFromOwner(Tag_Player_State_Active_Barrier);

	// 배열 초기화
	CurrentBarrierBlocks.Empty();

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}