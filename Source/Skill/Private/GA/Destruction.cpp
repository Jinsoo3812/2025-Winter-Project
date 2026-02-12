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
    if (!BlockSystem || !DestructionEffectClass || !DamageEffectClass)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // 1. 블록 파괴 (기존 유지)
    const TArray<FBlockReference>& TargetBlocks = PreviewTask->GetCurrentHighlightedBlocks();
    if (TargetBlocks.Num() > 0)
    {
        ApplyGameplayEffectToTargets(TargetBlocks, DestructionEffectClass);
    }

    bool bHitAnyEnemy = false;

    if (GetOwningActorFromActorInfo()->HasAuthority())
    {
        AActor* Avatar = GetAvatarActorFromActorInfo();

        // [보강 1] 위치 및 회전 동기화
        // 캐릭터의 회전값(Quat)을 가져와 박스를 캐릭터가 보는 방향으로 정렬합니다.
        FQuat AttackRotation = Avatar->GetActorQuat();

        // [보강 2] 오프셋 적용
        // 구조체에 정의된 RelativeOffset을 캐릭터의 회전에 맞춰 계산하여 실제 공격 중심점을 잡습니다.
        FVector AttackLocation = Avatar->GetActorLocation() + AttackRotation.RotateVector(PreviewRange.RelativeOffset);

        // [보강 3] 제자리 스윕 버그 방지
        // 시작과 끝이 완전히 같으면 무시될 수 있으므로, 아주 미세하게(0.1cm) 위로 이동시킵니다.
        FVector SweepEnd = AttackLocation + FVector(0.0f, 0.0f, 0.1f);

        FCollisionShape AttackShape = FCollisionShape::MakeBox(PreviewRange.Dimensions);

        TArray<FHitResult> HitResults;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(Avatar);
        Params.bTraceComplex = true; // 뼈(Physics Asset) 정밀 검사 활성화

        // 스윕 실행 (회전값과 미세 이동 적용)
        bool bHit = GetWorld()->SweepMultiByChannel(
            HitResults,
            AttackLocation,
            SweepEnd,      // 미세하게 이동된 끝점
            AttackRotation, // 캐릭터가 바라보는 방향으로 박스 회전
            ECC_Enemy,
            AttackShape,
            Params
        );

        if (bHit)
        {
            UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

            // 룬 배율 계산 로직 (기존 유지)
            float RuneDamageMultiplier = 1.0f;
            if (USkillComponent* SkillComp = Avatar->FindComponentByClass<USkillComponent>())
            {
                FGameplayTag MySlotTag;
                if (FGameplayAbilitySpec* MySpec = GetCurrentAbilitySpec())
                {
                    for (const FGameplayTag& Tag : MySpec->GetDynamicSpecSourceTags())
                    {
                        if (Tag.ToString().Contains("Skill.Slot")) { MySlotTag = Tag; break; }
                    }
                }
                if (MySlotTag.IsValid()) RuneDamageMultiplier = SkillComp->GetTotalRuneMultiplier(MySlotTag, ERuneType::Red);
            }

            // 중복 방지 및 필터링
            TMap<AActor*, FHitResult> UniqueHits;
            for (const FHitResult& Hit : HitResults)
            {
                AActor* HitActor = Hit.GetActor();
                if (!HitActor) continue;

                UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
                if (TargetASC && EnemyTag.IsValid() && TargetASC->HasMatchingGameplayTag(EnemyTag))
                {
                    if (!UniqueHits.Contains(HitActor)) UniqueHits.Add(HitActor, Hit);
                }
            }

            // 데미지 적용
            for (auto& Elem : UniqueHits)
            {
                AActor* TargetActor = Elem.Key;
                FHitResult& HitInfo = Elem.Value;
                UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

                if (TargetASC && SourceASC)
                {
                    bHitAnyEnemy = true;
                    FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
                    ContextHandle.AddHitResult(HitInfo);

                    FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);
                    if (SpecHandle.IsValid())
                    {
                        SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), BaseDamage);
                        SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.RuneMultiplier")), RuneDamageMultiplier);
                        SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

                        // 성공 로그: 어떤 뼈를 맞췄는지 확인 가능
                        UE_LOG(LogTemp, Warning, TEXT("[Destruction] Hit Confirmed: %s, Bone: %s"), *TargetActor->GetName(), *HitInfo.BoneName.ToString());
                    }
                }
            }
        }
    }

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