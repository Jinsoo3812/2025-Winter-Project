#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AttackRange.generated.h"

// 전방 선언 (컴파일 속도 향상)
class ABlockBase;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

/**
 * [범용 범위 공격 어빌리티 - 최종 수정본]
 * 1. 루트 모션 오차 해결: 예고(Telegraph) 시점의 위치를 기억(Cache)해서 공격 시 사용.
 * 2. 공중 부양 해결: 캡슐의 절반 높이를 계산해 정확한 '발바닥' 높이를 찾음.
 * 3. 색상 변경: 태그(Tag) 시스템을 통해 BlockBase와 통신.
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
	// =================================================================
	// [설정] 블루프린트에서 세팅할 변수들
	// =================================================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	// 몽타주 노티파이에서 보내는 '예고' 태그 (예: Event.Montage.Telegraph)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Tags")
	FGameplayTag TelegraphEventTag;

	// 몽타주 노티파이에서 보내는 '타격' 태그 (예: Event.Montage.Hit)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Tags")
	FGameplayTag HitEventTag;

	// 공격 사거리 (박스 길이)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackRangeForward = 300.0f;

	// 공격 좌우 폭
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackWidth = 300.0f;

	// 보스 앞에서 얼마나 떨어져서 공격이 시작될지 (오프셋)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackForwardOffset = 150.0f;

	// 데미지 이펙트 (GameplayEffect)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Effect")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 예고 동작 시 몽타주 재생 속도 (0.1 = 매우 느림)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timing")
	float TelegraphPlayRate = 0.1f;

	// 안전장치 타이머 (노티파이 누락 대비)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timing")
	float SafetyDuration = 1.5f;

	// =================================================================
	// [내부 상태 변수]
	// =================================================================

	FTimerHandle TimerHandle_SafetyRestore;

	// 색상을 변경한 블록들을 추적 (나중에 끄기 위해 저장)
	TArray<TWeakObjectPtr<ABlockBase>> AffectedBlocks;

	// [중요] 예고 단계에서 계산된 공격 위치를 저장하는 변수 (루트 모션 오차 방지)
	FVector CachedTargetLocation;

	// =================================================================
	// [Tasks]
	// =================================================================
	UPROPERTY()
	UAbilityTask_PlayMontageAndWait* MontageTask;

	UPROPERTY()
	UAbilityTask_WaitGameplayEvent* WaitTelegraphTask;

	UPROPERTY()
	UAbilityTask_WaitGameplayEvent* WaitHitTask;

	// =================================================================
	// [함수]
	// =================================================================

	UFUNCTION()
	void OnTelegraphEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageEnded();

	void ExecuteAttack();
	void RestoreMontageSpeed();
	void ResetBlockColors();

	// 공격 박스 계산 함수 (bIsTelegraph에 따라 저장/로드 수행)
	void CalculateAttackBox(FVector& OutCenter, FVector& OutExtent, bool bIsTelegraph);
};