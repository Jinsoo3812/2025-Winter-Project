#include "SkillSlotWidget.h"
#include "GameplayEffect.h"

void USkillSlotWidget::SetAbilitySystemComponent(UAbilitySystemComponent* InASC)
{
	Super::SetAbilitySystemComponent(InASC);

	// ASC와 태그가 유효한지 체크
	if (AbilitySystemComponent && CooldownTag.IsValid())
	{
		// 1. 태그 변경 이벤트 등록
		AbilitySystemComponent->RegisterGameplayTagEvent(
			CooldownTag,
			EGameplayTagEventType::NewOrRemoved
		).AddUObject(this, &USkillSlotWidget::HandleCooldownTagChanged);

		// 2. 초기 상태 체크 (이미 쿨타임 중일 경우)
		if (AbilitySystemComponent->GetTagCount(CooldownTag) > 0)
		{
			HandleCooldownTagChanged(CooldownTag, 1);
		}
	}
}

void USkillSlotWidget::HandleCooldownTagChanged(const FGameplayTag InTag, int32 NewCount)
{
	// 쿨타임 시작 (태그 갯수 > 0)
	if (NewCount > 0 && AbilitySystemComponent)
	{
		float TimeRemaining = 0.f;
		float Duration = 0.f;

		// 1. 우리가 찾는 쿨타임 태그로 쿼리 생성
		const FGameplayTagContainer QueryTags(CooldownTag);

		// 2. 해당 태그를 가진 모든 활성 이펙트(Active Effect)의 핸들을 가져옴
		TArray<FActiveGameplayEffectHandle> Handles =
			AbilitySystemComponent->GetActiveEffectsWithAllTags(QueryTags);

		// 3. 핸들을 순회하며 실제 정보를 찾음
		for (const FActiveGameplayEffectHandle& Handle : Handles)
		{
			// 핸들을 통해 실제 이펙트 데이터(ActiveGE)에 접근
			if (const FActiveGameplayEffect* ActiveGE =
				AbilitySystemComponent->GetActiveGameplayEffect(Handle))
			{
				// 4. 현재 월드 시간을 기준으로 남은 시간 직접 계산
				TimeRemaining = ActiveGE->GetTimeRemaining(
					AbilitySystemComponent->GetWorld()->GetTimeSeconds()
				);
				Duration = ActiveGE->GetDuration();

				// 쿨타임 이펙트는 보통 하나이므로 찾으면 바로 종료
				break;
			}
		}

		// UI 업데이트
		OnCooldownStarted(TimeRemaining, Duration);
	}
	// 쿨타임 종료 (태그 갯수 == 0)
	else
	{
		OnCooldownEnded();
	}
}