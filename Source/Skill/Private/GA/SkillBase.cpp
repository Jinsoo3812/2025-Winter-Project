// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/SkillBase.h"
#include "AbilitySystemComponent.h"
#include "BlockSystemInterface.h"
#include "BlockGameplayTags.h"
#include "SkillGameplayTags.h"

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
		UE_LOG(LogTemp, Error, TEXT("GA_SkillBase: World is null in ActivateAbility"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 프리뷰 상태인 다른 스킬 취소
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	if (ASC->HasMatchingGameplayTag(TAG_Skill_Preview))
	{
		FGameplayTagContainer PreviewTags(TAG_Skill_Preview);
		ASC->CancelAbilities(&PreviewTags);
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