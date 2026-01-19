// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GA/GA_SkillBase.h"
#include "GA_Explosive.generated.h"

class AExplosive;
class UAbilityTask_WaitInputPress;
class ABlockBase;

/**
 * 단발성 폭발물 투척 스킬.
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
	// --- 기능 함수 ---

	// 매 틱마다 호출되어 조준선(프리뷰) 업데이트
	UFUNCTION()
	void UpdatePreview();

	// 좌클릭 시 호출 (투척 확정)
	UFUNCTION()
	void OnLeftClickPressed();

	// 취소 키 입력 시 호출
	UFUNCTION()
	void OnCancelPressed(float TimeWaited);

	// 실제 폭발물 생성 및 투척 처리 로직
	void SpawnExplosive();

	// 하이라이트(프리뷰) 초기화
	void ClearHighlights();


	// --- 변수 및 설정 ---

	// 조준 업데이트를 위한 타이머
	UPROPERTY()
	FTimerHandle TickTimerHandle;

	// 입력 대기 태스크
	UPROPERTY()
	UAbilityTask_WaitInputPress* InputTask;

	// 생성할 폭발물 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Explosive")
	TSubclassOf<AExplosive> ExplosiveClass;

	// 폭발 반경
	UPROPERTY(EditDefaultsOnly, Category = "Explosive")
	float ExplosionRadius = 300.0f;

	// 파괴 시 적용할 이펙트 (구조물 파괴용)
	UPROPERTY(EditDefaultsOnly, Category = "Explosive")
	TSubclassOf<UGameplayEffect> DestructionEffect;


	// --- 상태 저장용 ---

	// 현재 프리뷰 중인(파란색) 블록 목록
	UPROPERTY()
	TArray<ABlockBase*> PreviewedBlocks;

	// 현재 마우스 오버된(초록색/타겟) 블록
	UPROPERTY()
	TWeakObjectPtr<ABlockBase> HighlightedBlock;

	// 투척 확정 시 저장된 타겟 블록
	UPROPERTY()
	TWeakObjectPtr<ABlockBase> SavedTargetBlock;
};
