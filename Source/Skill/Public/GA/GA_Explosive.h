// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GA/GA_SkillBase.h"
#include "GA_Explosive.generated.h"

class AExplosive;
class ABlockBase;
class UAbilityTask_WaitInputPress;

/**
 * 원거리 블록에 폭발물을 던지고, 재시전하여 폭발시키는 어빌리티
 */
UCLASS()
class SKILL_API UGA_Explosive : public UGA_SkillBase
{
	GENERATED_BODY()

public:
	UGA_Explosive();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	// 매 프레임 프리뷰 업데이트
	void UpdatePreview();

	// 좌클릭 시 호출 (폭발물 투척)
	UFUNCTION()
	void OnLeftClickPressed();

	// 취소 키(스킬 키) 입력 시 호출
	UFUNCTION()
	void OnCancelPressed(float TimeWaited);

	// 폭발물 투척 로직
	void SpawnExplosive();

	// 최종 폭발 처리 및 종료
	void PerformDetonateAndEnd();

	UFUNCTION()
	// 폭발물의 폭발 알림 수신
	void OnExplosiveDetonated();

	// 프리뷰 및 하이라이트 정리
	void ClearHighlights();

protected:
	// 폭발물 액터 클래스 (BP 주입)
	UPROPERTY(EditDefaultsOnly, Category = "Explosive")
	TSubclassOf<AExplosive> ExplosiveClass;

	// 자동 폭파 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Explosive")
	float AutoDetonateDelay = 3.0f;

	// 폭발 반지름 (구 형태)
	UPROPERTY(EditDefaultsOnly, Category = "Explosive")
	float ExplosionRadius = 300.0f;

	// 폭발 시 적용할 파괴 Effect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosive")
	TSubclassOf<UGameplayEffect> DestructionEffect;

	// 생성된 폭발물 인스턴스 (InstancedPerActor 정책이므로 멤버 변수 유지 가능)
	UPROPERTY()
	TArray<TWeakObjectPtr<AExplosive>> ExplosivesList;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Explosive")
	int32 MaxBombCount = 3;

	// 3개를 모두 던져서 기폭 대기 상태인지 확인하는 플래그
	bool bIsDetonationReady = false;

	// 프리뷰 타이머 핸들
	FTimerHandle TickTimerHandle;

	// 태스크 참조 보관
	UPROPERTY()
	UAbilityTask_WaitInputPress* InputTask;

	// 현재 투척 목표로 하이라이트된 블록 (매 프레임 변할 수 있음)
	UPROPERTY()
	TWeakObjectPtr<ABlockBase> HighlightedBlock;

	// 머티리얼 복구를 위해 저장
	UPROPERTY()
	TWeakObjectPtr<UMaterialInterface> OriginalMaterial;

	// 프리뷰 중이거나 하이라이트 효과가 적용된 블록들을 관리하는 배열
	TArray<ABlockBase*> PreviewedBlocks;

	// 투척 확정 시 타겟팅된 블록을 저장 (착탄 후 빨간색 표시를 위해 필요)
	TWeakObjectPtr<ABlockBase> SavedTargetBlock;
};
