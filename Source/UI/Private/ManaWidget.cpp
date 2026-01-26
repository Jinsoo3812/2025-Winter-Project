#include "ManaWidget.h"
#include "Winter2025PlayerAttributeSet.h"

void UManaWidget::SetAbilitySystemComponent(UAbilitySystemComponent* InASC)
{
	Super::SetAbilitySystemComponent(InASC);

	if (AbilitySystemComponent)
	{
		// 마나 변경 감지
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UWinter2025PlayerAttributeSet::GetManaAttribute()
		).AddUObject(this, &UManaWidget::HandleManaChanged);

		// 최대 마나 변경 감지
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UWinter2025PlayerAttributeSet::GetMaxManaAttribute()
		).AddUObject(this, &UManaWidget::HandleMaxManaChanged);

		// 초기값 갱신
		float CurrentMana = AbilitySystemComponent->GetNumericAttribute(UWinter2025PlayerAttributeSet::GetManaAttribute());
		float CurrentMaxMana = AbilitySystemComponent->GetNumericAttribute(UWinter2025PlayerAttributeSet::GetMaxManaAttribute());

		OnManaChanged(CurrentMana);
		OnMaxManaChanged(CurrentMaxMana);
	}
}

void UManaWidget::HandleManaChanged(const FOnAttributeChangeData& Data)
{
	OnManaChanged(Data.NewValue);
}

void UManaWidget::HandleMaxManaChanged(const FOnAttributeChangeData& Data)
{
	OnMaxManaChanged(Data.NewValue);
}