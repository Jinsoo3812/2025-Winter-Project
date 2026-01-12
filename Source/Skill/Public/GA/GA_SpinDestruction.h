// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GA/GA_SkillBase.h"
#include "GA_SpinDestruction.generated.h"

class UGameplayEffect;
class UAbilityTask_WaitInputPress;

/**
 * 회전하며 주변 적들에게 지속 데미지를 주는 파괴 스킬
 * GA_Destruction의 초록 룬(값 1.0) 변형 버전
 */
UCLASS(Blueprintable)
class SKILL_API UGA_SpinDestruction : public UGA_SkillBase
{
	GENERATED_BODY()
	
public:
	UGA_SpinDestruction();

	// GA 활성화 시 호출되는 함수
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

protected:
	// 범위에서 적용할 Gameplay Effect (파괴 효과)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay Effect")
	TSubclassOf<UGameplayEffect> DestructionEffect;

	// 회전 스킬 지속 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin Settings")
	float SpinDuration = 5.0f;

	// 회전 스킬의 원형 반지름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin Settings")
	float SpinRadius = 300.0f;

	// 데미지 적용 주기 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin Settings")
	float DamageTickInterval = 0.5f;

	// 스킬 키 재입력 감지용 Ability Task
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> WaitInputTask;

	// 회전 스킬 타이머 핸들들
	FTimerHandle SpinDurationTimerHandle;
	FTimerHandle DamageTickTimerHandle;
	FTimerHandle DebugDrawTimerHandle;

	// 회전 스킬 시작
	void StartSpin();

	// 회전 스킬 종료
	void StopSpin();

	// 회전 스킬의 데미지 적용 (주기적 호출)
	void ApplySpinDamage();

	// 회전 스킬 디버그 드로우 업데이트
	void UpdateDebugDraw();

	// 스킬 키 재입력 콜백 (조기 종료)
	UFUNCTION()
	void OnSkillKeyPressed(float TimeWaited);
};
