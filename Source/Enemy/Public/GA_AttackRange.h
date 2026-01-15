#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AttackRange.generated.h"

// 헤더를 직접 포함하지 않고 전방 선언만 합니다.
// cpp 파일에서만 include 하면 됩니다.
class ABlockBase;
class UAnimMontage;

/**
 * 범용 범위 공격 어빌리티
 */
UCLASS()
class ENEMY_API UGA_AttackRange : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AttackRange();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	// 설정 변수들 (블루프린트에서 수정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timing")
	float TelegraphDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackRangeForward = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Range")
	float AttackWidth = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Effect")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// Weak Ptr로 블록 참조 저장
	TArray<TWeakObjectPtr<ABlockBase>> AffectedBlocks;

	UFUNCTION()
	void ExecuteAttack();

	UFUNCTION()
	void OnMontageFinished();

	void ResetBlockColors();
};