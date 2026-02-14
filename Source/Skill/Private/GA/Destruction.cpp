// Fill out your copyright notice in the Description page of Project Settings.


#include "Destruction.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "PreviewTask.h"
#include "AbilitySystemBlueprintLibrary.h"

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

	// 프리뷰 태스크 시작 (박스 범위 시각화)
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
}	

void UDestruction::OnConfirmEventReceived(FGameplayEventData Payload)
{
	if (!BlockSystem || !DestructionEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Destruction: Missing BlockSystem or GE Class"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// PreviewTask로부터 블록 및 적 정보 가져오기
	const TArray<FBlockReference>& TargetBlocks = PreviewTask->GetCurrentHighlightedBlocks();
	const TArray<TWeakObjectPtr<AActor>>& TargetEnemies = PreviewTask->GetOverlappedEnemies();

	if (TargetBlocks.IsEmpty() && TargetEnemies.IsEmpty())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddSourceObject(this);

	FGameplayEffectSpecHandle DamageSpecHandle = ASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);
	FGameplayEffectSpecHandle DestructionSpecHandle = ASC->MakeOutgoingSpec(DestructionEffectClass, 1.0f, ContextHandle);

	// GE 적용
	if (TargetBlocks.Num() > 0 && DestructionSpecHandle.IsValid())
	{
		ApplyGameplayEffectToTargets(TargetBlocks, DestructionSpecHandle);
	}
	if (TargetEnemies.Num() > 0 && DamageSpecHandle.IsValid())
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

		ApplyGameplayEffectToTargets(TargetEnemies, DamageSpecHandle);
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