#include "BaseWidget.h"

void UBaseWidget::SetAbilitySystemComponent(UAbilitySystemComponent* InASC)
{
	if (InASC)
	{
		AbilitySystemComponent = InASC;
		// ASC가 들어오면 BP 이벤트를 호출하여 초기화 기회를 줌
		OnAbilitySystemComponentSet();
	}
}