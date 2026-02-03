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
		Tag_Highlight_Cursor,
		PreviewVisualizerClass,
		false
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
	// 1. 필수 요소 확인
	if (!BlockSystem || !DestructionEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Destruction: Missing BlockSystem or GE Class"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 2. PreviewTask가 이미 계산해둔 하이라이트 목록 가져오기
	const TArray<FBlockReference>& Targets = PreviewTask->GetCurrentHighlightedBlocks();

	if (Targets.Num() <= 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 3. GE 적용 로직 
	UAbilitySystemComponent* MyASC = GetAbilitySystemComponentFromActorInfo();
	FGameplayEffectContextHandle ContextHandle = MyASC->MakeEffectContext();
	ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());

	FGameplayEffectSpecHandle SpecHandle = MyASC->MakeOutgoingSpec(DestructionEffectClass, 1.0f, ContextHandle);

	if (SpecHandle.IsValid())
	{
		int32 HitCount = 0;
		for (const FBlockReference& Ref : Targets)
		{
			if (AActor* TargetActor = Cast<AActor>(Ref.TargetObject.Get()))
			{
				if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
				{
					if (TargetASC->HasMatchingGameplayTag(TargetRequiredTag))
					{
						MyASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
						HitCount++;
					}
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("Destruction: Successfully destroyed %d blocks from Preview list."), HitCount);
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