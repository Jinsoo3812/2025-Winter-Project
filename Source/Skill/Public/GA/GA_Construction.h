// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Construction.generated.h"

class ABlockBase;
class ADestructibleBlock;

/**
 * 블록 생성 스킬 - 스킬 키로 활성화하여 범위 내 블록 위에 새 블록을 생성
 */
UCLASS()
class SKILL_API UGA_Construction : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Construction();

	// 어빌리티 활성화
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 어빌리티 종료
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 생성 가능 범위 (XY 평면 반지름)
	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	float ConstructionRangeXY = 500.0f;

	// 생성 가능 범위 (Z축 위아래)
	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	float ConstructionRangeZ = 200.0f;

	// 생성할 블록 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	TSubclassOf<ADestructibleBlock> BlockToSpawn;

	// 프리뷰로 사용할 블록 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	TSubclassOf<AActor> PreviewBlockClass;

	// 프리뷰 블록 인스턴스
	UPROPERTY()
	TObjectPtr<AActor> PreviewBlock;

	// 하이라이트된 블록들 (그 위에 블록을 생성할 수 있는 블록들의 집합)
	// TSet 사용으로 Contains 연산이 O(1) 해시 조회로 최적화됨
	UPROPERTY()
	TSet<TObjectPtr<ABlockBase>> HighlightedBlocks;

	// 원본 머티리얼 저장 (복구용)
	UPROPERTY()
	TMap<TObjectPtr<ABlockBase>, TObjectPtr<UMaterialInterface>> OriginalMaterials;

	// 하이라이트 머티리얼
	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	TObjectPtr<UMaterialInterface> HighlightMaterial;

	// 타이머 핸들
	FTimerHandle TickTimerHandle;

	// 마지막 플레이어 위치 (하이라이트 업데이트 최적화용)
	FVector LastPlayerLocation;

	// 범위 내 블록들을 찾아서 하이라이트
	void HighlightBlocksInRange();

	// 하이라이트 제거
	void ClearHighlights();

	// 마우스 커서 아래 블록 찾기 및 프리뷰 업데이트
	void UpdatePreview();

	// 블록 생성
	void SpawnBlock();
};
