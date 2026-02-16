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

	virtual void StartPreview() override;

protected:
	// -------------------------------------------------------------------
	// 설정 변수
	// -------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	FGameplayTag BlockTagToSpawn;

	UPROPERTY(EditDefaultsOnly, Category = "Construction")
	TSubclassOf<AActor> BlockClassToSpawn;

	// -------------------------------------------------------------------
	// 이벤트 핸들러
	// -------------------------------------------------------------------
	UFUNCTION()
	void OnConfirmEventReceived(FGameplayEventData Payload);

	// -------------------------------------------------------------------
	// 프리뷰
	// -------------------------------------------------------------------
	// 태스크 포인터 저장
	UPROPERTY()
	UPreviewTask* PreviewTask;
};
