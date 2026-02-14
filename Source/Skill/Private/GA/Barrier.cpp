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

	StartPreview();

	// 초기 회전값 설정 (캐릭터가 바라보는 방향 기준 90도 스냅)
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		float Yaw = Avatar->GetActorRotation().Yaw;
		// 90도 단위 스냅 (0, 90, 180, 270)
		float SnappedYaw = FMath::GridSnap(Yaw, 90.0f);
		CurrentPreviewRotation = FRotator(0, SnappedYaw, 0);
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

void UBarrier::StartPreview()
{
	Super::StartPreview();

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
}

void UBarrier::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (PreviewTask)
	{
		// 프리뷰 태그 제거
		RemoveAbilityTag(Tag_Skill_State_Preview);

		// 태스크 종료
		PreviewTask->EndTask();
		PreviewTask = nullptr;
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

		// 시전자에서 목표 지점으로 향하는 벡터 계산
		FVector Dir = TargetLoc - PlayerLoc;
		Dir.Z = 0.0f; // 높이 차이 무시 (수평 방향만 고려)

		// 가장 지배적인 축(Dominant Axis)을 찾아 4방향으로 스냅
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

	// 프리뷰 액터에게 소환할 수 있는 블록 위치 목록 요청
	TArray<FTransform> SpawnTransforms = PreviewActor->GetValidSpawnData();

	//  실제 소환 진행
	if (SpawnTransforms.Num() > 0)
	{
		if (BlockSystem)
		{	
			// [추가] Payload 객체 생성 (NewObject 사용)
			// Outer를 this(Ability)로 설정하여 Ability가 살아있는 동안은 안전하게 유지
			UBarrierSpawnPayload* BarrierPayload = NewObject<UBarrierSpawnPayload>(this);

			// 데이터 채우기 (Barrier 스킬의 멤버 변수값들을 전달)
			// *주의: UBarrier 클래스에 아래 변수들이 정의되어 있어야 함
			BarrierPayload->LaunchSpeed = this->LaunchSpeed;
			BarrierPayload->MaxLifeTime = this->MaxLifeTime;
			BarrierPayload->TeamAllyTag = this->TeamAllyTag; // 예: Team.Ally
			BarrierPayload->TeamEnemyTag = this->TeamEnemyTag; // 예: Team.Enemy
			BarrierPayload->AllyKnockbackStrength = this->AllyKnockbackStrength;

			if (DamageEffectClass)
			{
				UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
				FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
				ContextHandle.AddSourceObject(this);

				FGameplayEffectSpecHandle DamageSpecHandle = ASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);

				if (DamageSpecHandle.IsValid())
				{
					// 데미지 룬 배율
					float RuneMultiplier = GetRuneMultiplier(ERuneType::Red);

					// SetByCaller 주입
					DamageSpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Damage, BaseDamage);
					DamageSpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_RuneMultiplier, RuneMultiplier);

					// SpecHandle을 Payload에 전달
					BarrierPayload->DamageSpecHandle = DamageSpecHandle;
				}
			}

			TArray<FBlockSpawnRequest> FinalRequests;
			for (auto& SpawnTransform : SpawnTransforms)
			{
				FBlockSpawnRequest Req(BlockTagToSpawn, SpawnTransform.GetLocation(), true, BarrierPayload);
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
			// 상호 물리적 무시 설정
			for (const auto& SourcePtr : CurrentBarrierBlocks)
			{
				AActor* SourceActor = SourcePtr.Get();
				if (!SourceActor) continue;

				// 충돌을 담당하는 PrimitiveComponent(루트) 가져오기
				UPrimitiveComponent* SourcePrim = Cast<UPrimitiveComponent>(SourceActor->GetRootComponent());
				if (!SourcePrim) continue;

				for (const auto& TargetPtr : CurrentBarrierBlocks)
				{
					AActor* TargetActor = TargetPtr.Get();

					// 자기 자신이 아니고 대상이 유효하다면
					if (TargetActor && SourceActor != TargetActor)
					{
						// 이동 중 충돌 검사에서 서로를 아예 제외시킴
						SourcePrim->IgnoreActorWhenMoving(TargetActor, true);
					}
				}
			}

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