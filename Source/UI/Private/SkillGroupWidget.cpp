#include "SkillGroupWidget.h"
#include "GameplayEffect.h" 

void USkillGroupWidget::SetAbilitySystemComponent(UAbilitySystemComponent* InASC)
{
	Super::SetAbilitySystemComponent(InASC);

	if (AbilitySystemComponent)
	{
		// --- 스킬 1 (Q) ---
		if (Skill1_Tag.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(Skill1_Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &USkillGroupWidget::HandleTagChanged_1);
			if (AbilitySystemComponent->GetTagCount(Skill1_Tag) > 0) HandleTagChanged_1(Skill1_Tag, 1);
		}

		// --- 스킬 2 (E) ---
		if (Skill2_Tag.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(Skill2_Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &USkillGroupWidget::HandleTagChanged_2);
			if (AbilitySystemComponent->GetTagCount(Skill2_Tag) > 0) HandleTagChanged_2(Skill2_Tag, 1);
		}

		// --- 스킬 3 (R) ---
		if (Skill3_Tag.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(Skill3_Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &USkillGroupWidget::HandleTagChanged_3);
			if (AbilitySystemComponent->GetTagCount(Skill3_Tag) > 0) HandleTagChanged_3(Skill3_Tag, 1);
		}

		// --- 스킬 4 (T) [추가됨] ---
		if (Skill4_Tag.IsValid())
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(Skill4_Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &USkillGroupWidget::HandleTagChanged_4);
			if (AbilitySystemComponent->GetTagCount(Skill4_Tag) > 0) HandleTagChanged_4(Skill4_Tag, 1);
		}
	}
}

// 연결 함수들
void USkillGroupWidget::HandleTagChanged_1(const FGameplayTag InTag, int32 NewCount) { ProcessCooldown(InTag, NewCount, 1); }
void USkillGroupWidget::HandleTagChanged_2(const FGameplayTag InTag, int32 NewCount) { ProcessCooldown(InTag, NewCount, 2); }
void USkillGroupWidget::HandleTagChanged_3(const FGameplayTag InTag, int32 NewCount) { ProcessCooldown(InTag, NewCount, 3); }
void USkillGroupWidget::HandleTagChanged_4(const FGameplayTag InTag, int32 NewCount) { ProcessCooldown(InTag, NewCount, 4); } // 추가됨

// 공통 처리 로직
void USkillGroupWidget::ProcessCooldown(FGameplayTag Tag, int32 Count, int32 SkillIndex)
{
	bool bIsCooldown = (Count > 0);
	float TimeRemaining = 0.f;
	float Duration = 0.f;

	if (bIsCooldown && AbilitySystemComponent)
	{
		const FGameplayTagContainer QueryTags(Tag);
		TArray<FActiveGameplayEffectHandle> Handles = AbilitySystemComponent->GetActiveEffectsWithAllTags(QueryTags);

		for (const FActiveGameplayEffectHandle& Handle : Handles)
		{
			if (const FActiveGameplayEffect* ActiveGE = AbilitySystemComponent->GetActiveGameplayEffect(Handle))
			{
				TimeRemaining = ActiveGE->GetTimeRemaining(AbilitySystemComponent->GetWorld()->GetTimeSeconds());
				Duration = ActiveGE->GetDuration();
				break;
			}
		}
	}

	// 인덱스에 따라 알맞은 BP 이벤트 발사
	switch (SkillIndex)
	{
	case 1: OnSkill1_Cooldown(bIsCooldown, TimeRemaining, Duration); break; // Q
	case 2: OnSkill2_Cooldown(bIsCooldown, TimeRemaining, Duration); break; // E
	case 3: OnSkill3_Cooldown(bIsCooldown, TimeRemaining, Duration); break; // R
	case 4: OnSkill4_Cooldown(bIsCooldown, TimeRemaining, Duration); break; // T (New!)
	}
}