// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GA/GA_Construction.h"
#include "GA_SummonBarrier.generated.h"

/**
 * 방벽 소환 스킬 (돌진 기능 추가)
 * HISM 기반 청크 시스템 호환 리팩토링 완료
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
	// 프리뷰 액터 (유령 방벽) 관리 배열
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> BarrierPreviewBlocks;

	// 실제 소환된 방벽 액터 관리 배열
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedBlocks;

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

	// 블록 사이즈 (BlockSystem에서 가져옴)
	float GridSize = 100.0f;

	// --- 오버라이드 함수들 ---

	// 프리뷰 업데이트 (소환할 블록 위치 계산 및 점유 확인)
	virtual void UpdatePreview() override;

	// 블록 생성 (블록 실제 생성 후 대기 모드 진입)
	virtual void SpawnBlock() override;

	// 입력 취소/재입력 처리
	virtual void OnCancelPressed(float TimeWaited);

	// 방벽 생성 후 돌진 전에는 취소 불가하도록 설정
	virtual bool CanBeCanceled() const override;

	// 좌클릭 이벤트 수신 처리
	virtual void OnLeftClickEventReceived(FGameplayEventData Payload) override;

private:
	// 중심 위치와 플레이어 위치를 기반으로 소환해야 할 방벽 위치(Transform) 배열 계산
	void CalculateBarrierTransforms(const FVector& CenterLocation, const FVector& PlayerLocation, TArray<FTransform>& OutTransforms);

	// 계산된 위치에 프리뷰 액터 배치 및 점유 확인
	void UpdateBarrierPreviewActors(const TArray<FTransform>& Transforms);

	// 돌진 시작 (두 번째 입력 시 호출)
	UFUNCTION()
	void StartBarrierCharge(float TimeWaited);

	// 매 프레임 방벽 이동 및 충돌 체크
	void TickBarrierCharge();
};