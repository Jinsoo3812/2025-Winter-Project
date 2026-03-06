// Fill out your copyright notice in the Description page of Project Settings.


#include "Destruction.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "PreviewTask.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "CollisionChannels.h"
#include "SkillComponent.h" 
#include "Rune/DA_Rune.h"


void UDestruction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	StartPreview();

	// 입력 대기 (좌클릭)
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

void UDestruction::StartPreview()
{
	Super::StartPreview();

	FSkillPreviewRange MultipliedPreviewRange = PreviewRange;
	MultipliedPreviewRange.Dimensions *= GetRuneMultiplier(ERuneType::Blue);

	// 프리뷰 태스크 시작 (박스 범위 시각화)
	PreviewTask = UPreviewTask::CreatePreviewTask(
		this,
		MultipliedPreviewRange, // 에디터에서 설정한 Box 범위 전달
		Tag_Highlight_Range,
		FGameplayTag::EmptyTag,
		PreviewVisualizerClass,
		nullptr
	);

	if (PreviewTask)
	{
		PreviewTask->ReadyForActivation();
	}
}	

void UDestruction::OnConfirmEventReceived(FGameplayEventData Payload)
{
    if (!BlockSystem || !DestructionEffectClass || !DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("Destruction: Missing BlockSystem or GE Class"));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!SourceASC || !Avatar)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 클라일 경우 서버의 ASC에게 클릭 이벤트를 RPC로 전달
    if (!Avatar->HasAuthority())
    {
        if (SkillComp)
        {
            // 서버 RPC: 클릭
            SkillComp->ServerSendSkillEvent(Tag_Event_Confirm);
        }

        // 클라이언트 측 예측(Prediction)을 위해 쿨타임만 즉시 돌리고 스킬 종료
        CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // 데미지 룬 배율 계산
    float RuneDamageMultiplier = 1.0f;
    if (SkillComp && GetCurrentAbilitySpec())
    {
        for (const FGameplayTag& Tag : GetCurrentAbilitySpec()->GetDynamicSpecSourceTags())
        {
            // "Skill.Slot" 관련 태그를 찾으면 룬 배율 계산 후 즉시 break
            if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Skill.Slot"))))
            {
                RuneDamageMultiplier = SkillComp->GetTotalRuneMultiplier(Tag, ERuneType::Red);
                break;
            }
        }
    }

    // 블록 파괴 로직 
    const TArray<FBlockReference>& TargetBlocks = PreviewTask->GetCurrentHighlightedBlocks();
    if (TargetBlocks.Num() > 0)
    {
        FGameplayEffectContextHandle BlockContextHandle = SourceASC->MakeEffectContext();
        BlockContextHandle.AddSourceObject(this);

        FGameplayEffectSpecHandle DestructionSpecHandle = SourceASC->MakeOutgoingSpec(DestructionEffectClass, 1.0f, BlockContextHandle);
        if (DestructionSpecHandle.IsValid())
        {
            ApplyGameplayEffectToTargets(TargetBlocks, DestructionSpecHandle);
        }
    }

    // 쿨타임 시작 (히트 여부와 상관없이 스킬이 발동되었으므로 쿨타임 적용)
    CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
    
    // 적군 피해 로직
    bool bHitAnyEnemy = false;

    // 3. 적 타격 로직 (정밀 Sweep 방식)
    if (Avatar->HasAuthority())
    {
        FQuat AttackRotation = Avatar->GetActorQuat();
        FVector AttackLocation = Avatar->GetActorLocation() + AttackRotation.RotateVector(PreviewRange.RelativeOffset);

        // 시작과 끝이 같으면 무시되는 현상 방지 (0.1 움직여서 Sweep 정상 동작)
        FVector SweepEnd = AttackLocation + FVector(0.0f, 0.0f, 0.1f);
        FCollisionShape AttackShape = FCollisionShape::MakeBox(PreviewRange.Dimensions);

        TArray<FHitResult> HitResults;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(Avatar);
        Params.bTraceComplex = true; // 뼈(Physics Asset) 정밀 검사

        // 스윕 실행
        bool bHit = GetWorld()->SweepMultiByChannel(
            HitResults,
            AttackLocation,
            SweepEnd,
            AttackRotation,
            ECC_Enemy,
            AttackShape,
            Params
        );

        if (bHit)
        {
            // 중복 방지 및 태그 필터링
            TMap<AActor*, FHitResult> UniqueHits;
            for (const FHitResult& Hit : HitResults)
            {
                AActor* HitActor = Hit.GetActor();
                if (!HitActor) continue;

                UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
                if (TargetASC && EnemyTag.IsValid() && TargetASC->HasMatchingGameplayTag(EnemyTag))
                {
                    if (!UniqueHits.Contains(HitActor)) {
                        UniqueHits.Add(HitActor, Hit);
                    }
                }
            }

            // 데미지 GE 적용
            for (auto& Elem : UniqueHits)
            {
                AActor* TargetActor = Elem.Key;
                FHitResult& HitInfo = Elem.Value;
                UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

                if (TargetASC)
                {
                    bHitAnyEnemy = true;

                    // 정밀한 HitResult를 Context에 포함
                    FGameplayEffectContextHandle HitContextHandle = SourceASC->MakeEffectContext();
                    HitContextHandle.AddHitResult(HitInfo);
                    HitContextHandle.AddSourceObject(this);

                    FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, HitContextHandle);
                    if (DamageSpecHandle.IsValid())
                    {
                        DamageSpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Damage, BaseDamage);
                        DamageSpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_RuneMultiplier, RuneDamageMultiplier);

                        SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);

                        UE_LOG(LogTemp, Warning, TEXT("[Destruction] Hit Confirmed: %s, Bone: %s"), *TargetActor->GetName(), *HitInfo.BoneName.ToString());
                    }
                }
            }
        }
    }
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UDestruction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveAbilityTag(Tag_Skill_State_Preview);

	if (PreviewTask)
	{
		PreviewTask->EndTask();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}