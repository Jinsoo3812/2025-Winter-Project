#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AttackRange.generated.h"

// 전방 선언 (헤더 의존성 최소화)
class ABlockBase;
class UAnimMontage;
class UAbilityTask_WaitGameplayEvent;

/**
 * [범용 범위 공격 어빌리티]
 * - 몽타주 기반의 N연타 공격을 지원합니다.
 * - AnimNotify로 타이밍을 정밀하게 제어합니다.
 */
UCLASS()
class ENEMY_API UGA_AttackRange : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AttackRange();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// =================================================================
	// [설정 변수] 블루프린트에서 수정 가능
	// =================================================================

	// 공격 애니메이션 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	// "장판 켜라"는 신호를 받을 태그 (예: Event.Montage.Telegraph)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	FGameplayTag TelegraphEventTag;

	// "때려라(데미지)"는 신호를 받을 태그 (예: Event.Montage.Hit)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	FGameplayTag HitEventTag;

	// 공격 범위 (길이)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackRangeForward = 300.0f;

	// 공격 범위 (너비)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackWidth = 300.0f;

	// [오프셋] 보스 몸체에서 얼마나 떨어진 곳에서 공격이 시작될지
	// 예: 200으로 설정하면, 보스 앞 200 거리부터 공격 범위가 시작됨
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackForwardOffset = 150.0f;

	// 적용할 데미지 이펙트 (GE)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Effect")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 몽타주가 없을 때 사용할 대기 시간 (Fallback 용도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timing")
	float TelegraphDuration = 1.5f;

	// 속도 복구를 위한 타이머 핸들
	FTimerHandle TimerHandle_SpeedUp;

	// 속도를 정상으로 돌리는 함수
	UFUNCTION()
	void RestoreMontageSpeed();

	// 예고(Telegraph) 시간 동안 몽타주 재생 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timing")
	float TelegraphPlayRate = 0.1f;


	// =================================================================
	// [내부 상태 변수]
	// =================================================================

	// 색상이 변경된 블록들을 기억해두는 배열 (나중에 끄기 위해)
	TArray<TWeakObjectPtr<ABlockBase>> AffectedBlocks;

	// =================================================================
	// [함수 선언]
	// =================================================================

	// [1] 장판 켜기 (Telegraph 활성화)
	UFUNCTION()
	void EnableTelegraph(FGameplayEventData Payload);

	// [2] 타격 신호 수신 (Hit 이벤트 발생 시 호출)
	UFUNCTION()
	void OnHitEventReceived(FGameplayEventData Payload);

	// [3] 실제 공격 판정 및 장판 끄기
	void ExecuteAttack();

	// [4] 몽타주 종료 시 호출
	UFUNCTION()
	void OnMontageFinished();

	// 블록 색상 초기화 헬퍼 함수
	void ResetBlockColors();

	// 예고(Telegraph) 당시의 공격 중심점을 기억할 변수
	FVector CachedTargetLocation;
};