// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GA/GA_SkillBase.h"
#include "GA_Destruction.generated.h"

class UGameplayEffect;
class UAbilityTask_WaitInputPress;

/**
 * 전방 직육면체(Box) 범위에 '파괴' 공격을 가하는 Gameplay Ability
 */
UCLASS(Blueprintable)
class SKILL_API UGA_Destruction : public UGA_SkillBase
{
	GENERATED_BODY()
	
public:
	UGA_Destruction();

	// GA 발동 시 호출되는 함수
	// Handle: GA 식별자
	// ActorInfo: GA를 소유한 액터(Owner, Avatar)의 정보
	// ActivationInfo: GA 발동에 대한 추가 정보 (예측, 서버 전용, 서버 대기 등)
	// TriggerEventData: GA가 GameplayEvent에 의해 발동된 경우의 이벤트 데이터. 입력/직접 호출의 경우 nullptr
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, 
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, 
		bool bReplicateEndAbility, 
		bool bWasCancelled
	) override;

protected:
	// 직육면체 범위의 전방 거리 (플레이어로부터 얼마나 앞에서 시전할지)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Range")
	float BoxDistance = 0.0f;

	// 직육면체 범위의 크기 (X: 전방 길이, Y: 좌우 폭, Z: 상하 높이)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Range")
	FVector BoxExtent = FVector(100.0f, 50.0f, 50.0f);

	// 블록에게 적용할 Gameplay Effect (파괴 효과)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay Effect")
	TSubclassOf<UGameplayEffect> DestructionEffect;

	// 프리뷰로 범위를 표시할 큐브 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	TSubclassOf<AActor> RangePreviewActorClass;

	// 프리뷰로 생성된 큐브 인스턴스
	UPROPERTY()
	TObjectPtr<AActor> RangePreviewActor;

	// 타이머 핸들
	FTimerHandle TickTimerHandle;

	// 스킬 키 재입력 감지를 위한 Ability Task
	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> WaitInputTask;

	// 디버그용 박스를 화면에 표시할 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	float DebugDrawDuration = 1.0f;

	// 매 프레임 프리뷰 위치 및 크기 업데이트
	void UpdatePreview();

	// 실제 파괴 로직 수행 (좌클릭 시 호출)
	void PerformDestruction();

	// 좌클릭 입력 콜백
	void OnLeftClickPressed();

	// 스킬 키 재입력 콜백 (취소)
	UFUNCTION()
	void OnCancelPressed(float TimeWaited);
};
