// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GA/GA_Construction.h"
#include "GA_SummonBarrier.generated.h"

class ADestructibleBlock;

/**
 * 방벽 소환 스킬 (돌진 기능 추가)
 * 1. 클릭 시 3x2 방벽 생성 (생성 후 스킬 유지)
 * 2. 다시 스킬 키 입력 시 방벽이 전방으로 돌진
 * 3. 각 블록은 개별적으로 장애물과 충돌 시 소멸
 */
UCLASS()
class SKILL_API UGA_SummonBarrier : public UGA_Construction
{
	GENERATED_BODY()

public:
	UGA_SummonBarrier();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 6개의 프리뷰 액터를 관리하기 위한 배열
	UPROPERTY()
	TArray<TObjectPtr<AActor>> BarrierPreviewBlocks;

	// 실제로 소환된 블록들을 관리하기 위한 배열 (돌진 시 이동시키기 위해)
	UPROPERTY()
	TArray<TObjectPtr<ADestructibleBlock>> SpawnedBlocks;

	// 블록 하나의 크기 (기본값 100)
	UPROPERTY(EditDefaultsOnly, Category = "Construction|Barrier")
	float BlockSize = 100.0f;

	// 돌진 속도 (cm/s)
	UPROPERTY(EditDefaultsOnly, Category = "Construction|Charge")
	float ChargeSpeed = 1500.0f;

	// 최대 돌진 거리
	UPROPERTY(EditDefaultsOnly, Category = "Construction|Charge")
	float MaxChargeDistance = 2000.0f;

	// 현재까지 이동한 거리
	float CurrentMovedDistance = 0.0f;

	// 돌진 방향
	FVector ChargeDirection;

	// 현재 돌진 중인지 여부
	bool bIsCharging = false;

	// 돌진 타이머 핸들
	FTimerHandle ChargeTimerHandle;

	// --- 오버라이드 함수들 ---

	// 프리뷰 업데이트 (6개 블록 위치 계산 및 점유 확인)
	virtual void UpdatePreview() override;

	// 블록 생성 (6개 블록 실제 생성 후 대기 모드 진입)
	virtual void SpawnBlock() override;

	// 입력 취소/재입력 처리 (W키 등)
	virtual void OnCancelPressed(float TimeWaited);

private:
	// 중심 위치와 플레이어 위치를 기반으로 6개 블록의 트랜스폼 계산
	void CalculateBarrierTransforms(const FVector& CenterLocation, const FVector& PlayerLocation, TArray<FTransform>& OutTransforms);

	// 프리뷰 액터 풀 관리
	void UpdateBarrierPreviewActors(const TArray<FTransform>& Transforms);

	// 해당 위치에 이미 BlockBase가 존재하는지 확인
	bool IsLocationOccupied(const FVector& Location) const;

	// 돌진 시작 (두 번째 입력 시 호출)
	UFUNCTION()
	void StartBarrierCharge(float TimeWaited);

	// 매 프레임 방벽 이동 처리
	void TickBarrierCharge();
};