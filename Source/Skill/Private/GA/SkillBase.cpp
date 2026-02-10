// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/SkillBase.h"
#include "AbilitySystemComponent.h"
#include "BlockSystemInterface.h"
#include "SkillGameplayTags.h"
#include "AbilitySystemComponent.h"

USkillBase::USkillBase()
{
	// GA를 소유한 액터마다 하나의 GA 객체만 생성
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	// 클라이언트에서 즉시 실행 후 서버가 검증
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void USkillBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// '프리뷰 중인' 다른 스킬이 있다면 모두 취소
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	if (ASC)
	{
		TArray<FGameplayAbilitySpec>& Specs = ASC->GetActivatableAbilities();

		// 반복문에서 구조체 참조(Spec&)를 사용합니다.
		for (const FGameplayAbilitySpec& Spec : Specs)
		{
			// 1. 활성화된(IsActive) 스킬이고
			// 2. 나 자신(this)이 아니며 (Spec.Ability로 접근)
			// 3. 동적 태그(DynamicAbilityTags)에 프리뷰 태그가 있는지 확인 (.으로 접근)
			if (Spec.IsActive() && Spec.Ability != this && Spec.DynamicAbilityTags.HasTag(TAG_Skill_State_Preview))
			{
				UE_LOG(LogTemp, Log, TEXT("SkillBase: Cancelling Previewing Skill: %s"), *Spec.Ability->GetName());

				// 핸들을 통해 강제 종료
				ASC->CancelAbilityHandle(Spec.Handle);
			}
		}
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	// BlockSystemInterface 캐싱
	if (UWorld* World = GetWorld())
	{
		if (!BlockSystem)
		{
			BlockSystem = IBlockSystemInterface::Get(World);
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("SkillBase: World is null in ActivateAbility"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
}

void USkillBase::InputPressed(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	
	if (HasAbilityTag(TAG_Skill_State_Preview))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void USkillBase::AddGameplayTagToOwner(const FGameplayTag& TagToAdd)
{
	if (!TagToAdd.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		// LooseTag 추가
		ASC->AddLooseGameplayTag(TagToAdd);
	}
}

void USkillBase::RemoveGameplayTagFromOwner(const FGameplayTag& TagToRemove)
{
	if (!TagToRemove.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(TagToRemove);
	}
}

void USkillBase::AddAbilityTag(const FGameplayTag& Tag)
{
	if (FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
	{
		Spec->DynamicAbilityTags.AddTag(Tag);
	}
}

void USkillBase::RemoveAbilityTag(const FGameplayTag& Tag)
{
	if (FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
	{
		Spec->DynamicAbilityTags.RemoveTag(Tag);
	}
}

bool USkillBase::HasAbilityTag(const FGameplayTag& Tag) const
{
	if (FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
	{
		return Spec->DynamicAbilityTags.HasTag(Tag);
	}
	return false;
}