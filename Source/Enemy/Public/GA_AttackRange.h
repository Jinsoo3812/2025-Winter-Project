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
 * [범용 범위 공격 어빌리티 - Standard Ver.]
 * - Push Capsule 덕분에 복잡한 사각지대 보정 로직 제거됨.
 * - 설정한 Offset 위치에 정확히 공격 박스를 생성.
 * - Tag 이벤트 기반 (순서: Wait -> Play)
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

	// 공격 사거리 (박스 길이)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackRangeForward = 300.0f;

	// 공격 폭 (박스 너비)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackWidth = 300.0f;

	// 공격 시작 위치 오프셋 (보스 중심에서 얼마나 떨어져서 생성할지)
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
	TArray<TWeakObjectPtr<ABlockBase>> AffectedBlocks;
	FVector CachedTargetLocation; // 판정 위치 기억용

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

	// 박스 계산 함수 (Standard)
	void CalculateAttackBox(FVector& OutCenter, FVector& OutExtent, bool bIsTelegraph);
};