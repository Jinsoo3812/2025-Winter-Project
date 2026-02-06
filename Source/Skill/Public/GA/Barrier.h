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
	// -------------------------------------------------------------------
	// Gameplay Tags
	// -------------------------------------------------------------------
	// 시전자에게 붙을 상태 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Player_State_Preview;

	// 스킬 인스턴스에 붙을 상태 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Skill_State_Preview;

	// 블록 하이라이트용 태그 (범위 표시)
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Highlight_Range;

	// 블록 하이라이트용 태그 (마우스 커서)
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Highlight_Cursor;

	// 입력 이벤트: 좌클릭(확인)
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Event_Confirm;

	// 입력 이벤트: 마우스 휠(회전)
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Event_Rotation;

	// -------------------------------------------------------------------------
	// 프리뷰
	// -------------------------------------------------------------------------
protected:
	// 프리뷰로 사용할 BP 클래스 (BP_BarrierPreview 할당)
	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	TSubclassOf<AActor> PreviewActorClass;

	// 태스크 포인터 저장
	UPROPERTY()
	UPreviewTask* PreviewTask;

	// -------------------------------------------------------------------------
	// 상태
	// -------------------------------------------------------------------------
protected:
	// 현재 프리뷰 회전 값 (90도 단위)
	FRotator CurrentPreviewRotation;

	// -------------------------------------------------------------------------
	// 이벤트 핸들러
	// -------------------------------------------------------------------------
protected:
	// 마우스 휠 이벤트 처리
	UFUNCTION()
	void OnMouseWheelEventReceived(FGameplayEventData Payload);

	// 클릭(Confirm) 이벤트 처리 (Construction의 것을 오버라이드하거나 새로 정의)
	UFUNCTION()
	void OnConfirmEventReceived(FGameplayEventData Payload);
};
