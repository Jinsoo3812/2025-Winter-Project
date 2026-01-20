#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AttackRange.generated.h"

// 전방 선언
class ABlockBase;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

/**
 * [범용 범위 공격 어빌리티 - Interface & Tag 버전]
 * - Push Box 덕분에 사각지대 보정 불필요 (Standard Math)
 * - BlockBase의 변경된 구조(Interface + Tag)를 사용하여 색상 변경
 */
UCLASS()
class ENEMY_API UGA_AttackRange : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AttackRange();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// --- 설정 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	FGameplayTag TelegraphEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	FGameplayTag HitEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackRangeForward = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackWidth = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackForwardOffset = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Effect")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timing")
	float TelegraphPlayRate = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timing")
	float SafetyDuration = 1.5f;

	// --- 내부 변수 ---
	FTimerHandle TimerHandle_SafetyRestore;

	// 색상을 변경한 블록들 추적
	TArray<TWeakObjectPtr<ABlockBase>> AffectedBlocks;
	FVector CachedTargetLocation;

	// --- Tasks ---
	UPROPERTY()
	UAbilityTask_PlayMontageAndWait* MontageTask;

	UPROPERTY()
	UAbilityTask_WaitGameplayEvent* WaitTelegraphTask;

	UPROPERTY()
	UAbilityTask_WaitGameplayEvent* WaitHitTask;

	// --- 함수 ---
	UFUNCTION()
	void OnTelegraphEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageEnded();

	void ExecuteAttack();
	void RestoreMontageSpeed();
	void ResetBlockColors();

	void CalculateAttackBox(FVector& OutCenter, FVector& OutExtent, bool bIsTelegraph);
};