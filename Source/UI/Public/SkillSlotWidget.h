#pragma once

#include "CoreMinimal.h"
#include "BaseWidget.h"
#include "GameplayTagContainer.h"
#include "SkillSlotWidget.generated.h"

/**
 * 스킬 슬롯 위젯
 */
UCLASS()
class UI_API USkillSlotWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// 에디터에서 지정할 쿨타임 태그
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Config")
	FGameplayTag CooldownTag;

	virtual void SetAbilitySystemComponent(UAbilitySystemComponent* InASC) override;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|UI")
	void OnCooldownStarted(float RemainingTime, float TotalDuration);

	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|UI")
	void OnCooldownEnded();

private:
	void HandleCooldownTagChanged(const FGameplayTag InTag, int32 NewCount);
};