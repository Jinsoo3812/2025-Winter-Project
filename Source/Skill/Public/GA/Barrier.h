// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GA/SkillBase.h"
#include "Barrier.generated.h"

/*
 * 
 */
UCLASS()
class SKILL_API UBarrier : public USkillBase
{
	GENERATED_BODY()
public:
	UBarrier();

	// -----------------------------------------------------------------------------
	// 스킬 GA의 시전, 취소 등 생명주기
	// -----------------------------------------------------------------------------
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// 실행 중인 어빌리티의 키 입력을 처리
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void StartPreview() override;

	void OnBlockDestroyed(FGameplayEventData Payload);
	// -------------------------------------------------------------------
	// Gameplay Tags
	// -------------------------------------------------------------------

	// 시전자에게 붙을 장벽 소환 상태 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Player_State_Active_Barrier;

	// 입력 이벤트: 마우스 휠(회전)
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Event_Rotation;

	// 소환할 블록 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag BlockTagToSpawn;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Barrier")
	TArray<FIntPoint> BarrierShapesByRuneCount;

	// -------------------------------------------------------------------------
	// 프리뷰
	// -------------------------------------------------------------------------
protected:
	// 프리뷰로 사용할 BP 클래스 (BP_BarrierPreview 할당)
	UPROPERTY(EditDefaultsOnly, Category = "Barrier")
	TSubclassOf<AActor> PreviewActorClass;

	// 태스크 포인터 저장
	UPROPERTY()
	UPreviewTask* PreviewTask;

	// 현재 소환된 장벽 블록들 (PerInstance 정책으로 유지)
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> CurrentBarrierBlocks;

	// 현재 프리뷰 회전 값 (90도 단위)
	FRotator CurrentPreviewRotation;

	// 블록 소환이 완료된 후 호출되는 함수
	void OnBlocksSpawned(const TArray<TWeakObjectPtr<AActor>>& SpawnedBlocks);

	// 룬 개수에 따라 로컬 오프셋 배열을 계산하는 내부 헬퍼 함수
	TArray<FVector> CalculateBarrierOffsetsByRune() const;

	// -------------------------------------------------------------------------
	// 발사
	// -------------------------------------------------------------------------
protected:
	// 발사 방향 저장
	FVector LaunchDirection = FVector::ForwardVector;

	// 발사 속도 
	UPROPERTY(EditDefaultsOnly, Category = "Launch")
	float LaunchSpeed = 1500.0f;

	// 발사 후 최대 수명
	UPROPERTY(EditDefaultsOnly, Category = "Launch")
	float MaxLifeTime = 3.0f;

	// 아군 넉백 힘
	UPROPERTY(EditDefaultsOnly, Category = "Launch")
	float AllyKnockbackStrength = 800.0f;

	// 적에게 적용할 데미지 GE 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Launch")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 발사 함수
	void Launch();

	// -------------------------------------------------------------------------
	// 입력 이벤트 처리
	// -------------------------------------------------------------------------
protected:
	// 마우스 휠 이벤트 처리
	UFUNCTION()
	void OnMouseWheelEventReceived(FGameplayEventData Payload);

	// 클릭(Confirm) 이벤트 처리 (Construction의 것을 오버라이드하거나 새로 정의)
	UFUNCTION()
	void OnConfirmEventReceived(FGameplayEventData Payload);
};
