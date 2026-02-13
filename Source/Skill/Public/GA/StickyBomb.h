// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GA/SkillBase.h"
#include "Bomb.h"
#include "StickyBomb.generated.h"

class UPreviewTask;

/**
 * 점착 폭탄 스킬.
 * 프리뷰 > 투척 > 폭발
 */
UCLASS()
class SKILL_API UStickyBomb : public USkillBase
{
	GENERATED_BODY()

protected:
	// -----------------------------------------------------------------------------
	// 스킬 설정
	// -----------------------------------------------------------------------------

	// 소환할 폭탄 액터 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<ABomb> BombClass;

	// 폭발 데미지를 줄 GE 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 폭탄 투척 체공 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	float ThrowDuration = 1.0f;

	// 폭탄 자동 폭발 시간 (0이면 무제한 대기)
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	float AutoDetonateTime = 3.0f;

	// 폭발 반경
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	float ExplosionRadius = 300.0f;

	// 폭탄이 날아갈 때 시전자에게 붙일 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Player_State_Bomb_Throwing;

	// 폭탄이 기폭 대기 중일 때 시전자에게 붙일 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Player_State_Bomb_Active;

	// 폭탄 부착 시 받을 이벤트 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Event_BombAttached;

	// 폭탄 폭발 시 받을 이벤트 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Event_BombDetonated;

	// -----------------------------------------------------------------------------
	// 내부 상태 관리
	// -----------------------------------------------------------------------------

	// 현재 관리 중인 폭탄
	TWeakObjectPtr<ABomb> CurrentBomb;

	// 프리뷰 태스크
	UPROPERTY()
	TObjectPtr<UPreviewTask> PreviewTask;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// 실행 중인 어빌리티의 키 입력을 처리
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	// -----------------------------------------------------------------------------
	// 단계별 로직
	// -----------------------------------------------------------------------------

	// Phase 1: 프리뷰 시작
	virtual void StartPreview() override;

	// Confirm 입력 시 호출 (던지기)
	UFUNCTION()
	void OnConfirmEventReceived(FGameplayEventData Payload);

	// Phase 2: 폭탄 투척
	void ThrowBomb(const FVector& TargetLocation);

	// Phase 3: 폭파 명령
	void CommandDetonate();

	// -----------------------------------------------------------------------------
	// 폭탄이 보내오는 이벤트 핸들러
	// -----------------------------------------------------------------------------

	// 폭탄이 어딘가에 부착되었을 때
	UFUNCTION()
	void OnBombAttached(FGameplayEventData Payload);

	// 폭탄이 터졌을 때 (부착 후 폭파, 혹은 날아가다 터짐 모두 포함)
	UFUNCTION()
	void OnBombDetonated(FGameplayEventData Payload);
};
