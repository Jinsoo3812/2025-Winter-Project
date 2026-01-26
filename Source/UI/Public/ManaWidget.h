#pragma once

#include "CoreMinimal.h"
#include "BaseWidget.h"
#include "GameplayEffectTypes.h"
#include "ManaWidget.generated.h"

/**
 * 마나 전용 위젯
 */
UCLASS()
class UI_API UManaWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	virtual void SetAbilitySystemComponent(UAbilitySystemComponent* InASC) override;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|UI")
	void OnManaChanged(float NewValue);

	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|UI")
	void OnMaxManaChanged(float NewValue);

private:
	void HandleManaChanged(const FOnAttributeChangeData& Data);
	void HandleMaxManaChanged(const FOnAttributeChangeData& Data);
};