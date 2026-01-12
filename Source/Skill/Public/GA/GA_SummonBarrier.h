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
	// N개의 프리뷰 액터를 관리하기 위한 배열
	UPROPERTY()
	TArray<TObjectPtr<AActor>> BarrierPreviewBlocks;

	// 실제로 소환된 블록들을 관리하기 위한 배열
	UPROPERTY()
	TArray<TObjectPtr<ADestructibleBlock>> SpawnedBlocks;

	// 돌진 속도
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

	// 블록 사이즈
	float GridSize = 100.0f;

	// --- 오버라이드 함수들 ---

	// 프리뷰 업데이트 (소환할 블록 위치 계산 및 점유 확인)
	virtual void UpdatePreview() override;

	// 블록 생성 (블록 실제 생성 후 대기 모드 진입)
	virtual void SpawnBlock() override;

	// 입력 취소/재입력 처리
	virtual void OnCancelPressed(float TimeWaited);

	// CancelAbility가 호출되면 취소당하는 함수의 CanBeCanceled로 결정함
	// '방벽 생성 && 아직 발사 안함' 상태에서는 취소되면 안되므로 이를 재정의
	// 취소 가능 여부를 판단하는 함수
	virtual bool CanBeCanceled() const override;

	// 좌클릭 입력 처리 (부모의 단일 프리뷰 확인 로직을 대체하기 위함)
	virtual void OnLeftClickPressed() override;

private:
	// 중심 위치와 플레이어 위치를 기반으로 소환해야 할 N개 블록의 위치 배열을 OutTransforms에 반환
	void CalculateBarrierTransforms(const FVector& CenterLocation, const FVector& PlayerLocation, TArray<FTransform>& OutTransforms);

	// 소환해야 할 N개 블록의 프리뷰 액터의 생성 관리 및 표시
	void UpdateBarrierPreviewActors(const TArray<FTransform>& Transforms);

	// 해당 위치에 이미 다른 블록이 존재하는지 확인
	bool IsLocationOccupied(const FVector& Location) const;

	// 돌진 시작 (두 번째 입력 시 호출)
	UFUNCTION()
	void StartBarrierCharge(float TimeWaited);

	// 매 프레임 방벽 이동 처리
	void TickBarrierCharge();
};
