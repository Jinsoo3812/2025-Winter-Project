// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GA/GA_SkillBase.h"
#include "GA_BuffBarrier.generated.h"


/**
 * 3단계로 나뉘어 발동되는 버프/장벽 스킬
 * 1. 스킬 사용 시 바닥에 범위 표시
 * 2. 가장자리 벽 생성 및 버프 적용
 * 3. 벽 제거 및 쿨타임 적용
 */
UCLASS()
class SKILL_API UGA_BuffBarrier : public UGA_SkillBase
{
	GENERATED_BODY()
	
public:
	UGA_BuffBarrier();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	// 생성할 벽 블록 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Barrier")
	TSubclassOf<class ABlockBase> WallBlockClass;

	// 적용할 버프 GE
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Barrier")
	TSubclassOf<class UGameplayEffect> BuffEffectClass;

	// 자동 전환 시간 (기본 3초)
	UPROPERTY(EditDefaultsOnly, Category = "Skill|Barrier")
	float AutoTransitionTime = 3.0f;

	UPROPERTY()
	FVector InstallCenterLocation;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	// 페이즈 진행 중 비용 검사 무시를 위해 오버라이드
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags) const override;

private:
	// 1단계에서 찾은 바닥 블록들 (InstancedPerActor 정책으로 인해 데이터 유지됨)
	UPROPERTY()
	TArray<TObjectPtr<AActor>> HighlightedBlocks;

	// 2단계에서 생성한 벽 블록들
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedWalls;

	// 자동 전환을 위한 타이머 핸들
	FTimerHandle AutoTransitionTimerHandle;

	// --- 내부 로직 함수들 ---

	// 1단계: 범위 표시 로직
	void ExecutePhase1_Highlight();

	// 2단계: 벽 생성 및 버프 로직
	void ExecutePhase2_Deploy();

	// 3단계: 해제 및 쿨타임 로직
	void ExecutePhase3_Cleanup();

	// 타이머에 의해 호출되어 다음 단계를 자동 실행하는 함수
	UFUNCTION()
	void OnAutoTransition();

	// 가장자리 블록 판별 로직
	void FindEdgeBlocks(const TArray<TObjectPtr<AActor>>& InBlocks, TArray<TObjectPtr<AActor>>& OutEdges);

	// 범위 내 아군에게 버프 적용
	void ApplyBuffToTargets();
};
