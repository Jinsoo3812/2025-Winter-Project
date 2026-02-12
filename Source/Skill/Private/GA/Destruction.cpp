// Fill out your copyright notice in the Description page of Project Settings.


#include "Destruction.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "PreviewTask.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Collision/CollisionChannels.h" //콜리전 정의용
#include "SkillComponent.h" 
#include "Rune/DA_Rune.h"


void UDestruction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 1. 프리뷰 모드 진입
	AddGameplayTagToOwner(Tag_State_Preview);
	AddAbilityTag(Tag_Skill_State_Preview);

	// 2. 프리뷰 태스크 시작 (박스 범위 시각화)
	PreviewTask = UPreviewTask::CreatePreviewTask(
		this,
		PreviewRange, // 에디터에서 설정한 Box 범위 전달
		Tag_Highlight_Range,
		FGameplayTag::EmptyTag,
		PreviewVisualizerClass,
		nullptr
	);

	if (PreviewTask)
	{
		PreviewTask->ReadyForActivation();
	}

	// 3. 입력 대기 (좌클릭)
	UAbilityTask_WaitGameplayEvent* WaitConfirm = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		Tag_Event_Confirm,
		nullptr,
		false,
		false
	);

	if (WaitConfirm)
	{
		WaitConfirm->EventReceived.AddDynamic(this, &UDestruction::OnConfirmEventReceived);
		WaitConfirm->ReadyForActivation();
	}
}

void UDestruction::OnConfirmEventReceived(FGameplayEventData Payload)
{
    // 1. 유효성 검사: 에디터에서 GE가 할당되었는지 확인
    if (!BlockSystem || !DestructionEffectClass || !DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("Destruction: 필수 클래스(Destruction/Damage GE)가 할당되지 않았습니다."));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 2. 지형 파괴 로직 (기존 유지)
    const TArray<FBlockReference>& TargetBlocks = PreviewTask->GetCurrentHighlightedBlocks();
    if (TargetBlocks.Num() > 0)
    {
        ApplyGameplayEffectToTargets(TargetBlocks, DestructionEffectClass);
    }

    // -------------------------------------------------------------------------
    // 3. 적 피격 판정 및 GE 적용 로직
    // -------------------------------------------------------------------------
    bool bHitAnyEnemy = false;

    // [멀티플레이] 서버에서만 판정 및 데미지 적용 수행
    if (GetOwningActorFromActorInfo()->HasAuthority())
    {
        // [판정] 사용자님께서 성공했던 원본 로직 그대로 Dimensions 사용
        FVector AttackLocation = GetAvatarActorFromActorInfo()->GetActorLocation();
        FCollisionShape AttackShape = FCollisionShape::MakeBox(PreviewRange.Dimensions);

        TArray<FHitResult> HitResults;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(GetAvatarActorFromActorInfo());

        // 스윕 실행
        bool bHit = GetWorld()->SweepMultiByChannel(
            HitResults,
            AttackLocation,
            AttackLocation,
            FQuat::Identity,
            ECC_Enemy,
            AttackShape,
            Params
        );

        if (bHit)
        {
            UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
            AActor* AvatarActor = GetAvatarActorFromActorInfo();

            // 룬 배율 계산 (빨간색 룬 합산)
            float RuneDamageMultiplier = 1.0f;
            if (USkillComponent* SkillComp = AvatarActor->FindComponentByClass<USkillComponent>())
            {
                FGameplayTag MySlotTag;
                if (FGameplayAbilitySpec* MySpec = GetCurrentAbilitySpec())
                {
                    for (const FGameplayTag& Tag : MySpec->GetDynamicSpecSourceTags())
                    {
                        if (Tag.ToString().Contains("Skill.Slot")) { MySlotTag = Tag; break; }
                    }
                }
                if (MySlotTag.IsValid())
                {
                    RuneDamageMultiplier = SkillComp->GetTotalRuneMultiplier(MySlotTag, ERuneType::Red);
                }
            }

            // [중복 타격 방지] 한 마리의 적(Actor)에게는 한 번만 GE 적용
            TMap<AActor*, FHitResult> UniqueHitActors;
            for (const FHitResult& Hit : HitResults)
            {
                AActor* HitActor = Hit.GetActor();
                if (!HitActor) continue;

                // 타겟의 ASC 확인 및 EnemyTag 필터링
                UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
                if (TargetASC && EnemyTag.IsValid() && TargetASC->HasMatchingGameplayTag(EnemyTag))
                {
                    if (!UniqueHitActors.Contains(HitActor))
                    {
                        UniqueHitActors.Add(HitActor, Hit);
                    }
                }
            }

            // [GE 적용] 선별된 타겟들에게 에디터에서 설정한 GE 적용
            for (auto& Elem : UniqueHitActors)
            {
                AActor* TargetActor = Elem.Key;
                FHitResult& HitInfo = Elem.Value;
                UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

                if (TargetASC && SourceASC)
                {
                    bHitAnyEnemy = true;

                    // 1. Context 생성 (맞은 부위 정보 포함)
                    FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
                    ContextHandle.AddHitResult(HitInfo);

                    // 2. Spec 생성: 헤더에 정의된 'DamageEffectClass'를 사용하여 명세서 생성
                    FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);

                    if (SpecHandle.IsValid())
                    {
                        // 3. 값 주입: 코드에 숫자를 직접 쓰지 않고 멤버 변수(BaseDamage) 사용
                        // (부모 클래스인 USkillBase에 정의된 BaseDamage 값이 GE로 전달됩니다)
                        SpecHandle.Data->SetSetByCallerMagnitude(
                            FGameplayTag::RequestGameplayTag(FName("Data.Damage")),
                            BaseDamage
                        );

                        // 룬 배율 주입
                        SpecHandle.Data->SetSetByCallerMagnitude(
                            FGameplayTag::RequestGameplayTag(FName("Data.RuneMultiplier")),
                            RuneDamageMultiplier
                        );

                        // 4. 최종 적용
                        SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

                        // [로그] 어떤 뼈를 맞췄는지 출력
                        UE_LOG(LogTemp, Warning, TEXT("[Destruction] Target: %s, Bone: %s"), *TargetActor->GetName(), *HitInfo.BoneName.ToString());
                    }
                }
            }
        }
    }

    // 종료 처리
    if (TargetBlocks.IsEmpty() && !bHitAnyEnemy)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UDestruction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveGameplayTagFromOwner(Tag_State_Preview);
	RemoveAbilityTag(Tag_Skill_State_Preview);

	if (PreviewTask)
	{
		PreviewTask->EndTask();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}