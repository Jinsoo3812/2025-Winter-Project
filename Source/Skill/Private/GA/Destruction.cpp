// Fill out your copyright notice in the Description page of Project Settings.


#include "Destruction.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "PreviewTask.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "CollisionChannels.h"
#include "SkillComponent.h" 
#include "Rune/DA_Rune.h"
#include "GameplayAbilityTargetData_Blocks.h"


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

    // GAS 표준: 이 아바타가 내 로컬 컨트롤러에 의해 움직이는가? / 이 환경이 서버인가?
    bool bIsLocallyControlled = CurrentActorInfo->IsLocallyControlled();
    bool bHasAuthority = Avatar->HasAuthority();

    // 서버가 RPC로 받은 데이터가 있다면 일단 담아둡니다.
    FGameplayAbilityTargetDataHandle TargetHandle = Payload.TargetData;

    // -------------------------------------------------------------------
    // 1. [Local] 로컬 조종 캐릭터 (클라이언트 or 리슨 서버) - 타겟 수집
    // -------------------------------------------------------------------
    if (bIsLocallyControlled)
    {
        TargetHandle.Clear(); // 내 화면에서 직접 수집할 것이므로 초기화

        if (SkillComp && PreviewTask)
        {
            // --- [Index 0] 블록 패킹 ---
            FGameplayAbilityTargetData_Blocks* BlockTargetData = new FGameplayAbilityTargetData_Blocks();
            BlockTargetData->Blocks = PreviewTask->GetCurrentHighlightedBlocks();
            TargetHandle.Add(BlockTargetData);

            // --- [Index 1 ~ N] 적군 정밀 타격 패킹 (Sweep) ---
            FQuat AttackRotation = Avatar->GetActorQuat();
            FVector AttackLocation = Avatar->GetActorLocation() + AttackRotation.RotateVector(PreviewRange.RelativeOffset);
            FVector SweepEnd = AttackLocation + FVector(0.0f, 0.0f, 0.1f);
            FCollisionShape AttackShape = FCollisionShape::MakeBox(PreviewRange.Dimensions);

            TArray<FHitResult> HitResults;
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(Avatar);
            Params.bTraceComplex = true;

            if (GetWorld()->SweepMultiByChannel(HitResults, AttackLocation, SweepEnd, AttackRotation, ECC_Enemy, AttackShape, Params))
            {
                TMap<AActor*, FHitResult> UniqueHits;
                for (const FHitResult& Hit : HitResults)
                {
                    AActor* HitActor = Hit.GetActor();
                    if (HitActor && !UniqueHits.Contains(HitActor))
                    {
                        UniqueHits.Add(HitActor, Hit);

                        FGameplayAbilityTargetData_SingleTargetHit* HitData = new FGameplayAbilityTargetData_SingleTargetHit();
                        HitData->HitResult = Hit;
                        TargetHandle.Add(HitData);
                    }
                }
            }
        }

        // '클라이언트'일 경우에만 서버로 RPC를 보내고 자기 자신은 (예측) 종료
        if (!bHasAuthority)
        {
            if (SkillComp)
            {
                SkillComp->ServerSendSkillEvent(Tag_Event_Confirm, TargetHandle);
            }
            CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
            return;
        }
    }

    // -------------------------------------------------------------------
    // 2. [Server] 권한을 가진 서버 (리슨 서버의 로컬 실행 or 클라이언트 RPC 수신)
    // -------------------------------------------------------------------
    if (bHasAuthority)
    {
        float RuneDamageMultiplier = 1.0f;
        if (SkillComp && GetCurrentAbilitySpec())
        {
            for (const FGameplayTag& Tag : GetCurrentAbilitySpec()->GetDynamicSpecSourceTags())
            {
                if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Skill.Slot"))))
                {
                    RuneDamageMultiplier = SkillComp->GetTotalRuneMultiplier(Tag, ERuneType::Red);
                    break;
                }
            }
        }

        // --- [Index 0] 블록 파괴 로직 적용 ---
        if (TargetHandle.IsValid(0))
        {
            const FGameplayAbilityTargetData* BaseData = TargetHandle.Get(0);
            if (BaseData && BaseData->GetScriptStruct() == FGameplayAbilityTargetData_Blocks::StaticStruct())
            {
                const FGameplayAbilityTargetData_Blocks* BlocksData = static_cast<const FGameplayAbilityTargetData_Blocks*>(BaseData);
                if (BlocksData->Blocks.Num() > 0)
                {
                    FGameplayEffectContextHandle BlockContextHandle = SourceASC->MakeEffectContext();
                    BlockContextHandle.AddSourceObject(this);

                    FGameplayEffectSpecHandle DestructionSpecHandle = SourceASC->MakeOutgoingSpec(DestructionEffectClass, 1.0f, BlockContextHandle);
                    if (DestructionSpecHandle.IsValid())
                    {
                        ApplyGameplayEffectToTargets(BlocksData->Blocks, DestructionSpecHandle);
                    }
                }
            }
        }

        // --- [Index 1 ~ N] 적군 데미지 로직 적용 ---
        for (int32 i = 1; i < TargetHandle.Data.Num(); ++i)
        {
            const FGameplayAbilityTargetData* BaseData = TargetHandle.Get(i);
            if (BaseData && BaseData->GetScriptStruct() == FGameplayAbilityTargetData_SingleTargetHit::StaticStruct())
            {
                const FGameplayAbilityTargetData_SingleTargetHit* HitData = static_cast<const FGameplayAbilityTargetData_SingleTargetHit*>(BaseData);
                const FHitResult& HitInfo = HitData->HitResult;
                AActor* TargetActor = HitInfo.GetActor();

                if (!TargetActor) continue;

                float Distance = FVector::Distance(Avatar->GetActorLocation(), TargetActor->GetActorLocation());
                if (Distance > PreviewRange.Dimensions.X + 300.0f) continue; // 핵 방지 거리 검증

                UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
                if (TargetASC && EnemyTag.IsValid() && TargetASC->HasMatchingGameplayTag(EnemyTag))
                {
                    FGameplayEffectContextHandle HitContextHandle = SourceASC->MakeEffectContext();
                    HitContextHandle.AddHitResult(HitInfo);
                    HitContextHandle.AddSourceObject(this);

                    FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, HitContextHandle);
                    if (DamageSpecHandle.IsValid())
                    {
                        DamageSpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Damage, BaseDamage);
                        DamageSpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_RuneMultiplier, RuneDamageMultiplier);

                        SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
                        UE_LOG(LogTemp, Log, TEXT("[Destruction] Hit Confirmed: %s, Bone: %s"), *TargetActor->GetName(), *HitInfo.BoneName.ToString());
                    }
                }
            }
        }

        // 서버 측 쿨타임 및 스킬 종료
        CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
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