// Fill out your copyright notice in the Description page of Project Settings.


#include "Destruction.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "PreviewTask.h"
#include "AbilitySystemBlueprintLibrary.h"

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

	// GE 적용
	if (TargetBlocks.Num() > 0)
	{
		ApplyGameplayEffectToTargets(TargetBlocks, DestructionEffectClass, DestructibleTag);
	}
	if (TargetEnemies.Num() > 0)
	{	
		UE_LOG(LogTemp, Log, TEXT("Destruction: Applying Damage GE to %d Enemies"), TargetEnemies.Num());
		ApplyGameplayEffectToTargets(TargetEnemies, DamageEffectClass, EnemyTag);
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