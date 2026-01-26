#include "HealthWidget.h"
#include "Winter2025PlayerAttributeSet.h" // 플레이어 어트리뷰트 헤더 필수!

void UHealthWidget::SetAbilitySystemComponent(UAbilitySystemComponent* InASC)
{
	// 1. 부모(BaseWidget)의 설정 로직 실행 (ASC 저장 등)
	Super::SetAbilitySystemComponent(InASC);

	// 2. ASC가 유효하다면 체력 변경 감지 시작
	if (AbilitySystemComponent)
	{
		// [체력 변경 감지]
		// 값이 변하면 HandleHealthChanged 함수를 호출하도록 등록
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UWinter2025PlayerAttributeSet::GetHealthAttribute()
		).AddUObject(this, &UHealthWidget::HandleHealthChanged);

		// [최대 체력 변경 감지]
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UWinter2025PlayerAttributeSet::GetMaxHealthAttribute()
		).AddUObject(this, &UHealthWidget::HandleMaxHealthChanged);

		// [초기값 갱신]
		// 위젯이 처음 떴을 때, 현재 체력을 바로 보여주기 위해 강제로 한 번 호출
		float CurrentHealth = AbilitySystemComponent->GetNumericAttribute(UWinter2025PlayerAttributeSet::GetHealthAttribute());
		float CurrentMaxHealth = AbilitySystemComponent->GetNumericAttribute(UWinter2025PlayerAttributeSet::GetMaxHealthAttribute());

		OnHealthChanged(CurrentHealth);
		OnMaxHealthChanged(CurrentMaxHealth);
	}
}

void UHealthWidget::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	// 변경된 값(NewValue)을 BP 이벤트로 전달
	OnHealthChanged(Data.NewValue);
}

void UHealthWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	OnMaxHealthChanged(Data.NewValue);
}