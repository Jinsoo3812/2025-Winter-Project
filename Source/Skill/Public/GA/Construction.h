// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SkillBase.h"
#include "Construction.generated.h"

class PreviewTask;

/**
 * 지정 위치에 블록 하나를 소환하는 스킬
 */
UCLASS()
class SKILL_API UConstruction : public USkillBase
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	// -------------------------------------------------------------------
	// 설정 변수
	// -------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	FGameplayTag BlockTagToSpawn;

	// -------------------------------------------------------------------
	// Gameplay Tags
	// -------------------------------------------------------------------
	// 시전자에게 붙을 상태 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_State_Preview;

	// 블록 하이라이트용 태그 (범위 표시)
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Highlight_Range;

	// 블록 하이라이트용 태그 (마우스 커서)
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Highlight_Cursor;

	// 입력 이벤트: 좌클릭(확인)
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Event_Confirm;

	// -------------------------------------------------------------------
	// 내부 로직 함수
	// -------------------------------------------------------------------
	UFUNCTION()
	void OnConfirmEventReceived(FGameplayEventData Payload);

	// 태스크 포인터 저장
	UPROPERTY()
	UPreviewTask* PreviewTask;
};
