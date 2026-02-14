// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/SkillBase.h"
#include "BlockSystemInterface.h"
#include "SkillGameplayTags.h"
#include "GameplayEffect.h"
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
			// Preview 상태인 GA가 있다면 취소
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
	if (!BlockSystem) {
		if (UWorld* World = GetWorld())
		{
			BlockSystem = IBlockSystemInterface::Get(World);
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("SkillBase: World is null in ActivateAbility"));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}


	// SkillComponent 캐싱
	if (!SkillComp) {
		if (AActor* OwnerActor = GetOwningActorFromActorInfo())
		{
			SkillComp = OwnerActor->FindComponentByClass<USkillComponent>();
		}
	}
}

void USkillBase::StartPreview()
{
	AddAbilityTag(Tag_Skill_State_Preview);
}

void USkillBase::InputPressed(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	
	if (HasAbilityTag(Tag_Skill_State_Preview))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

const FGameplayTagContainer* USkillBase::GetCooldownTags() const
{
	FGameplayTagContainer* MutableTags = const_cast<FGameplayTagContainer*>(&TempCooldownTags);
	MutableTags->Reset();

	if (CooldownTag.IsValid())
	{
		MutableTags->AddTag(CooldownTag);
	}

	return MutableTags;
}

float USkillBase::GetRuneMultiplier(ERuneType RuneType) const
{
	if (!SkillComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillBase: SkillComp is null in GetCooldownRuneMultiplier"));	
		return 1.0f;
	}

	float RuneMultiplier = 1.0f;

	// 내 슬롯 태그를 이용해 룬 배율 가져오기
	if (const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
	{
		for (const FGameplayTag& Tag : Spec->DynamicAbilityTags)
		{
			if (Tag.MatchesTag(TAG_Skill_Slot))
			{
				// 배율 요청 (쿨타임은 Yellow)
				RuneMultiplier = SkillComp->GetTotalRuneMultiplier(Tag, RuneType);
				break;
			}
		}
	}

	return RuneMultiplier;
}

int32 USkillBase::GetRuneCount(ERuneType RuneType) const
{
	if (!SkillComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillBase: SkillComp is null in GetRuneCount"));	
		return 0;
	}
	int32 RuneCount = 0;
	// 내 슬롯 태그를 이용해 룬 개수 가져오기
	if (const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
	{
		for (const FGameplayTag& Tag : Spec->DynamicAbilityTags)
		{
			if (Tag.MatchesTag(TAG_Skill_Slot))
			{
				RuneCount = SkillComp->GetTotalRuneCount(Tag, RuneType);
				break;
			}
		}
	}
	return RuneCount;
}

void USkillBase::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE) return;

	// Spec 생성
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CooldownGE->GetClass(), GetAbilityLevel());
	if (!SpecHandle.IsValid()) return;

	// Cooldown GE Spec에 쿨타임 태그 주입 
	if (CooldownTag.IsValid())
	{
		SpecHandle.Data.Get()->DynamicGrantedTags.AddTag(CooldownTag);
	}

	// 쿨타임 시간 주입 (SetByCaller)
	SpecHandle.Data.Get()->SetSetByCallerMagnitude(TAG_Data_Cooldown, BaseCooldown * GetRuneMultiplier(ERuneType::Yellow));

	// 시전자에게 적용
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
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
		if (ASC->HasMatchingGameplayTag(TagToRemove))
		{
			ASC->RemoveLooseGameplayTag(TagToRemove);
		}
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