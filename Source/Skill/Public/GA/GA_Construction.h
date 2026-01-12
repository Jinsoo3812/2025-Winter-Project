// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA/GA_SkillBase.h"
#include "GA_Construction.generated.h"

class ABlockBase;
class ADestructibleBlock;
class UAbilityTask_WaitInputPress;

/**
 * 블록 건설 스킬 - 스킬 키로 활성화하여 범위 내 블록을 찾고 생성할 수 있음
 */
UCLASS()
class SKILL_API UGA_Construction : public UGA_SkillBase
{
	GENERATED_BODY()

public:
	UGA_Construction();

	// 어빌리티 활성화
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 어빌리티 종료
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 생성할 블록 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	TSubclassOf<ADestructibleBlock> BlockToSpawn;

	// 프리뷰로 표시할 블록 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	TSubclassOf<AActor> PreviewBlockClass;

	// 프리뷰로 생성된 블록 인스턴스
	UPROPERTY()
	TObjectPtr<AActor> PreviewBlock;

	// 타이머 핸들
	FTimerHandle TickTimerHandle;

	// W키 재입력 감지를 위한 Ability Task
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> WaitInputTask;

	// 프리뷰 중이거나 하이라이트 효과가 적용된 블록들을 관리하는 배열
	TArray<TWeakObjectPtr<ABlockBase>> PreviewedBlocks;

	// 범위 내 블록들을 찾아서 하이라이트
	void HighlightBlocksInRange();

	// 하이라이트 제거
	void ClearHighlights();

	// 마우스 커서 아래 블록 찾기 및 프리뷰 업데이트
	virtual void UpdatePreview();

	// 블록 생성
	virtual void SpawnBlock();

	// 좌클릭 입력 콜백
	virtual void OnLeftClickPressed();

	// W키 재입력 콜백 (스킬 취소)
	UFUNCTION()
	void OnCancelPressed(float TimeWaited);
};
