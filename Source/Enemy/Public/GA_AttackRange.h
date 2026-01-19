#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AttackRange.generated.h"

// 전방 선언 (헤더 의존성 최소화)
class ABlockBase;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

/**
 * [범용 범위 공격 어빌리티]
 * - 몽타주 기반의 N연타 공격 지원
 * - AnimNotify(Telegraph/Hit)를 통한 정밀한 타이밍 제어
 * - UPROPERTY Task 보호를 통한 GC 방지 (안정성 강화)
 */
UCLASS()
class ENEMY_API UGA_AttackRange : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AttackRange();

	// [생명주기] 어빌리티 시작
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// [생명주기] 어빌리티 종료 (중요: 여기서 Task 정리)
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// =================================================================
	// [설정 변수] 블루프린트에서 수정 가능
	// =================================================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	FGameplayTag TelegraphEventTag; // 예: Event.Montage.Telegraph

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	FGameplayTag HitEventTag;       // 예: Event.Montage.Hit

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackRangeForward = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackWidth = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackForwardOffset = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Effect")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timing")
	float TelegraphDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timing")
	float TelegraphPlayRate = 0.1f;

	// =================================================================
	// [내부 상태 관리]
	// =================================================================

	FTimerHandle TimerHandle_SpeedUp;

	// 장판 블록 관리 (WeakPtr로 안전하게 참조)
	TArray<TWeakObjectPtr<ABlockBase>> AffectedBlocks;

	// 예고(Telegraph) 당시의 공격 중심점 (루트 모션으로 인한 오차 방지)
	FVector CachedTargetLocation;

	// =================================================================
	// [Task 멤버 변수] UPROPERTY로 보호하여 GC(가비지 컬렉션) 방지
	// =================================================================

	UPROPERTY()
	UAbilityTask_PlayMontageAndWait* MontageTask;

	UPROPERTY()
	UAbilityTask_WaitGameplayEvent* WaitHitTask;

	UPROPERTY()
	UAbilityTask_WaitGameplayEvent* WaitTelegraphTask;

	// =================================================================
	// [함수 선언]
	// =================================================================

	UFUNCTION()
	void RestoreMontageSpeed();

	UFUNCTION()
	void EnableTelegraph(FGameplayEventData Payload);

	UFUNCTION()
	void OnHitEventReceived(FGameplayEventData Payload);

	void ExecuteAttack();

	UFUNCTION()
	void OnMontageFinished();

	void ResetBlockColors();
};