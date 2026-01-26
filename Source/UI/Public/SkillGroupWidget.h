#pragma once

#include "CoreMinimal.h"
#include "BaseWidget.h"
#include "GameplayTagContainer.h"
#include "SkillGroupWidget.generated.h"

/**
 * 스킬 그룹 위젯 (통짜 관리용)
 * 스킬 Q, E, R, T (4개)의 쿨타임을 관리합니다.
 */
UCLASS()
class UI_API USkillGroupWidget : public UBaseWidget
{
	GENERATED_BODY()

public:
	// --- [설정] 에디터에서 지정할 태그들 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Config")
	FGameplayTag Skill1_Tag; // Q

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Config")
	FGameplayTag Skill2_Tag; // E

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Config")
	FGameplayTag Skill3_Tag; // R

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Config")
	FGameplayTag Skill4_Tag; // T (New!)

	virtual void SetAbilitySystemComponent(UAbilitySystemComponent* InASC) override;

protected:
	// --- [블루프린트로 쏴줄 이벤트들] ---

	// 스킬 1번 (Q)
	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|UI")
	void OnSkill1_Cooldown(bool bIsCooldown, float RemainingTime, float Duration);

	// 스킬 2번 (E)
	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|UI")
	void OnSkill2_Cooldown(bool bIsCooldown, float RemainingTime, float Duration);

	// 스킬 3번 (R)
	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|UI")
	void OnSkill3_Cooldown(bool bIsCooldown, float RemainingTime, float Duration);

	// 스킬 4번 (T) - 추가됨
	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|UI")
	void OnSkill4_Cooldown(bool bIsCooldown, float RemainingTime, float Duration);

private:
	// 내부 로직 함수들
	void HandleTagChanged_1(const FGameplayTag InTag, int32 NewCount);
	void HandleTagChanged_2(const FGameplayTag InTag, int32 NewCount);
	void HandleTagChanged_3(const FGameplayTag InTag, int32 NewCount);
	void HandleTagChanged_4(const FGameplayTag InTag, int32 NewCount); // 추가됨

	// 쿨타임 계산 도우미 함수
	void ProcessCooldown(FGameplayTag Tag, int32 Count, int32 SkillIndex);
};