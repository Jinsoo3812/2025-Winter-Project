// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA/GA_SkillBase.h"
#include "GA_Construction.generated.h"

class ABlockBase;
class ADestructibleBlock;
class UMaterialInstanceDynamic;
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
	// 건설 가능 범위 (XY 평면 반지름)
	UPROPERTY(EditDefaultsOnly, Category = "Range")
	float ConstructionRangeXY = 500.0f;

	// 건설 가능 범위 (Z축 위아래)
	UPROPERTY(EditDefaultsOnly, Category = "Range")
	float ConstructionRangeZ = 200.0f;

	// 생성할 블록 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	TSubclassOf<ADestructibleBlock> BlockToSpawn;

	// 프리뷰로 표시할 블록 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	TSubclassOf<AActor> PreviewBlockClass;

	// 프리뷰로 생성된 블록 인스턴스
	UPROPERTY()
	TObjectPtr<AActor> PreviewBlock;

	// 하이라이트된 블록들 (그 위에 블록을 생성할 수 있는 블록들의 목록)
	// TSet 자료구조 Contains 메서드 O(1) 해시 탐색을 최적화함
	UPROPERTY()
	TSet<TObjectPtr<ABlockBase>> HighlightedBlocks;

	// 각 블록별로 동적으로 머티리얼 인스턴스를 저장
	// 이미 하이라이트된 블록들을 재사용하여 중복 생성 방지
	UPROPERTY()
	TMap<TObjectPtr<ABlockBase>, TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;

	// 각 블록별로 원본 머티리얼 저장
	UPROPERTY()
	TMap<TObjectPtr<ABlockBase>, TObjectPtr<UMaterialInterface>> OriginalMaterials;

	// 각 블록별로 원본 EmissivePower 값 저장
	UPROPERTY()
	TMap<TObjectPtr<ABlockBase>, float> OriginalEmissivePowers;

	// 하이라이트 시 적용할 EmissivePower 값
	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	float HighlightEmissivePower = 0.1f;

	// 타이머 핸들
	FTimerHandle TickTimerHandle;

	// 이전의 플레이어 위치 (하이라이트 업데이트 최적화용)
	FVector LastPlayerLocation;

	// W키 재입력 감지를 위한 Ability Task
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> WaitInputTask;

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
