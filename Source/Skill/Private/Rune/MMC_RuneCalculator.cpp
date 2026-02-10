// Fill out your copyright notice in the Description page of Project Settings.


#include "Rune/MMC_RuneCalculator.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "SkillGameplayTags.h"

float UMMC_RuneCalculator::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	// GA Spec 및 시전자 ASC
	const FGameplayAbilitySpec* SourceAbilitySpec = Spec.GetEffectContext().GetAbilityInstance_NotReplicated()->GetCurrentAbilitySpec();
	UAbilitySystemComponent* SourceASC = Spec.GetContext().GetOriginalInstigatorAbilitySystemComponent();

	if (!SourceAbilitySpec || !SourceASC)
	{
		return 1.0f; // 기본값 1배
	}

	// GA의 소속 스킬 슬롯을 DynamicAbilityTags에서 찾기
	FGameplayTag SlotTag = FGameplayTag::EmptyTag;
	for (const FGameplayTag& Tag : SourceAbilitySpec->DynamicAbilityTags)
	{
		if (Tag.MatchesTag(TAG_Skill_Slot))
		{
			SlotTag = Tag;
			break;
		}
	}

	if (!SlotTag.IsValid())
	{
		return 1.0f; // 슬롯 정보가 없으면 적용 안 함
	}

	// 검색할 태그 조합
	// "Skill.Slot.1" + "Rune.Red" = "Skill.Slot.1.Rune.Red"
	FString TargetTagString = FString::Printf(TEXT("%s.%s"), *SlotTag.ToString(), *TargetRuneTypeTag.ToString());
	FGameplayTag FullTag = FGameplayTag::RequestGameplayTag(FName(*TargetTagString), false);

	// Source(플레이어)에게 해당 태그가 몇 개 있는지 카운트
	int32 RuneCount = 0;
	if (FullTag.IsValid())
	{
		RuneCount = SourceASC->GetTagCount(FullTag);
	}

	// 커브 테이블에서 배율 가져오기
	float Multiplier = 1.0f;
	if (RuneCurveInfo.CurveTable)
	{
		// 커브에서 X값(RuneCount)에 해당하는 Y값(Multiplier)을 가져옴
		static const FString ContextString(TEXT("MMC_RuneBonus"));
		Multiplier = RuneCurveInfo.Eval(RuneCount, ContextString);
	}
	else
	{
		// 커브가 없으면 임시 공식 (개당 10% 증가) - 테스트용
		Multiplier = 1.0f + (RuneCount * 0.1f);
	}

	return Multiplier;
}