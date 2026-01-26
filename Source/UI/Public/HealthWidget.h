#pragma once

#include "CoreMinimal.h"
#include "BaseWidget.h"
#include "GameplayEffectTypes.h"
#include "HealthWidget.generated.h"


UCLASS()
class UI_API UHealthWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	virtual void SetAbilitySystemComponent(UAbilitySystemComponent* InASC) override;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|UI")
	void OnHealthChanged(float NewValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|UI")
	void OnMaxHealthChanged(float NewValue);

private:
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
};