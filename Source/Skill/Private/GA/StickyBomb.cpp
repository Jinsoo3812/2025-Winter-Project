// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/StickyBomb.h"
#include "PreviewTask.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "BlockSystemInterface.h"

void UStickyBomb::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	StartPreview();

	// 클릭(Confirm) 대기
	UAbilityTask_WaitGameplayEvent* WaitConfirm = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		Tag_Event_Confirm,
		nullptr,
		false,
		false
	);
	WaitConfirm->EventReceived.AddDynamic(this, &UStickyBomb::OnConfirmEventReceived);
	WaitConfirm->ReadyForActivation();
}

void UStickyBomb::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	// 폭탄이 날아가는 중이라면 입력 무시
	if (ASC->HasMatchingGameplayTag(Tag_Player_State_Bomb_Throwing))
	{
		return;
	}

	// 폭탄이 활성화(부착)된 상태에서 키를 다시 눌렀다면 기폭
	if (ASC && ASC->HasMatchingGameplayTag(Tag_Player_State_Bomb_Active))
	{
		CommandDetonate();
	}
}

void UStickyBomb::StartPreview()
{
	Super::StartPreview();

	// 프리뷰 태스크 생성
	PreviewTask = UPreviewTask::CreatePreviewTask(
		this,
		PreviewRange,
		Tag_Highlight_Range, // 범위 하이라이트 태그
		Tag_Highlight_Cursor, // 커서 하이라이트 태그
		nullptr, // Visualizer Class
		nullptr  // GhostBlock Class
	);

	if (PreviewTask)
	{
		PreviewTask->ReadyForActivation();
	}

}

void UStickyBomb::OnConfirmEventReceived(FGameplayEventData Payload)
{
	// 프리뷰 종료
	if (PreviewTask)
	{
		// 커서 블록 위치 가져오기 
		FBlockReference TargetBlock = PreviewTask->GetCurrentCursorBlock();

		PreviewTask->EndTask();
		PreviewTask = nullptr;

		RemoveAbilityTag(Tag_Skill_State_Preview);

		if (BlockSystem && TargetBlock.IsValid())
		{
			// 목표 지점 계산 (블록 위쪽)
			FVector TargetLoc = BlockSystem->GetBlockLocation(TargetBlock);
			// 블록 중앙보다 살짝 위를 목표로 잡음
			TargetLoc.Z += BlockSystem->GetGridSize() * 0.5f;

			ThrowBomb(TargetLoc);
		}
		else
		{
			// 유효하지 않은 곳 클릭 시 취소
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		}
	}
}

void UStickyBomb::ThrowBomb(const FVector& TargetLocation)
{
	if (!BombClass)
	{
		UE_LOG(LogTemp, Error, TEXT("StickyBomb: BombClass is not set!"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	// 시전자에게 Throwing 태그 추가 -> 입력 무시 상태
	AddGameplayTagToOwner(Tag_Player_State_Bomb_Throwing);

	// 폭탄 스폰
	FTransform SpawnTransform;
	SpawnTransform.SetLocation(Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 100.0f); // 캐릭터 약간 앞
	SpawnTransform.SetRotation(Avatar->GetActorRotation().Quaternion());

	ABomb* SpawnedBomb = GetWorld()->SpawnActorDeferred<ABomb>(BombClass, SpawnTransform, Avatar, Cast<APawn>(Avatar));

	if (SpawnedBomb)
	{
		// Payload 구성
		FBombPayload Payload(
			ExplosionRadius * GetRuneMultiplier(ERuneType::Blue),
			AutoDetonateTime,
			GetAbilitySystemComponentFromActorInfo(),
			Tag_Event_BombAttached,
			Tag_Event_BombDetonated,
			BombHighlightTag,
			BlockSystem
		);

		// Payload 주입 및 스폰 완료
		SpawnedBomb->InitializeExplosive(Payload);
		SpawnedBomb->FinishSpawning(SpawnTransform);

		// 물리 발사 (포물선)
		float GravityZ = GetWorld()->GetGravityZ();
		SpawnedBomb->LaunchByTime(TargetLocation, ThrowDuration, GravityZ);

		// 관리 포인터 저장
		CurrentBomb = SpawnedBomb;

		// 폭탄 이벤트 리스너 등록

		// 부착 성공 이벤트 (Attached)
		UAbilityTask_WaitGameplayEvent* WaitAttached = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			Tag_Event_BombAttached, // Bomb.cpp에서 StickToTarget 할 때 보내줘야 함 (추가 필요)
			nullptr,
			false,
			false
		);
		WaitAttached->EventReceived.AddDynamic(this, &UStickyBomb::OnBombAttached);
		WaitAttached->ReadyForActivation();

		// 폭발 이벤트 (Detonated)
		UAbilityTask_WaitGameplayEvent* WaitDetonated = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			Tag_Event_BombDetonated,
			nullptr,
			false,
			false
		);
		WaitDetonated->EventReceived.AddDynamic(this, &UStickyBomb::OnBombDetonated);
		WaitDetonated->ReadyForActivation();

		// *주의: EndAbility를 호출하지 않음. 이벤트를 기다림.
	}
}

void UStickyBomb::OnBombAttached(FGameplayEventData Payload)
{
	// '던지는 중' 태그 제거 -> 이제 입력 무시 상태 해제
	RemoveGameplayTagFromOwner(Tag_Player_State_Bomb_Throwing);

	// '활성화(설치됨)' 태그 추가 -> 이제 누르면 터짐
	AddGameplayTagToOwner(Tag_Player_State_Bomb_Active);
}

void UStickyBomb::CommandDetonate()
{
	// 사용자가 스킬 키를 눌러서 폭파 명령을 내림
	if (CurrentBomb.IsValid())
	{
		CurrentBomb->Detonate();
	}
	else
	{
		// 폭탄이 사라졌는데 태그가 남아있는 경우 (예외 처리)
		RemoveGameplayTagFromOwner(Tag_Player_State_Bomb_Active);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UStickyBomb::OnBombDetonated(FGameplayEventData Payload)
{
	// 폭탄이 터졌음 (자폭했든, 적에 맞았든, 내가 터트렸든)

	// 모든 상태 태그 정리
	RemoveGameplayTagFromOwner(Tag_Player_State_Bomb_Throwing);
	RemoveGameplayTagFromOwner(Tag_Player_State_Bomb_Active);

	if (DamageEffectClass && Payload.TargetData.Num() > 0)
	{
		// TargetData에서 피해자 명단(AActor*) 추출
		TArray<AActor*> TargetActors;
		for (int32 i = 0; i < Payload.TargetData.Num(); ++i)
		{
			const FGameplayAbilityTargetData* Data = Payload.TargetData.Get(i);
			if (Data)
			{
				// 포장된 배열을 풀어서 실제 액터 포인터로 변환
				TArray<TWeakObjectPtr<AActor>> Actors = Data->GetActors();
				for (TWeakObjectPtr<AActor> WeakActor : Actors)
				{
					if (WeakActor.IsValid())
					{
						TargetActors.Add(WeakActor.Get());
					}
				}
			}
		}

		// 폭발 시점 기준의 GE Spec 생성
		if (TargetActors.Num() > 0)
		{
			UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
			FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
			ContextHandle.AddSourceObject(this);

			FGameplayEffectSpecHandle DamageSpecHandle = ASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);
			FGameplayEffectSpecHandle DestructionSpecHandle = ASC->MakeOutgoingSpec(DestructionEffectClass, 1.0f, ContextHandle);

			// Damage GE 적용
			if (DamageSpecHandle.IsValid())
			{
				// 룬 배율 계산
				float RuneMultiplier = 1.0f;
				if (SkillComp && GetCurrentAbilitySpec())
				{
					for (const FGameplayTag& Tag : GetCurrentAbilitySpec()->DynamicAbilityTags)
					{
						if (Tag.MatchesTag(TAG_Skill_Slot))
						{
							RuneMultiplier = SkillComp->GetTotalRuneMultiplier(Tag, ERuneType::Red);
							break;
						}
					}
				}

				// SetByCaller 주입
				DamageSpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Damage, BaseDamage);
				DamageSpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_RuneMultiplier, RuneMultiplier);

				ApplyGameplayEffectToTargets(TargetActors, DamageSpecHandle);
			}

			// Destruction GE 적용
			if (DestructionSpecHandle.IsValid())
			{
				ApplyGameplayEffectToTargets(TargetActors, DestructionSpecHandle);
			}
		}
	}

	// 쿨타임 시작 (폭발 시점부터 쿨타임)
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);

	// 스킬 종료
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UStickyBomb::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 혹시 프리뷰가 켜진 상태로 끝났다면 정리
	if (PreviewTask)
	{
		PreviewTask->EndTask();
		PreviewTask = nullptr;
		RemoveAbilityTag(Tag_Skill_State_Preview);
	}

	// 상태 태그 정리
	RemoveGameplayTagFromOwner(Tag_Player_State_Bomb_Throwing);
	RemoveGameplayTagFromOwner(Tag_Player_State_Bomb_Active);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}