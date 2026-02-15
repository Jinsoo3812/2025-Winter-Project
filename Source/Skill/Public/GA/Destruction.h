// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SkillBase.h"
#include "PreviewTask.h"
#include "Destruction.generated.h"


class UGameplayEffect;
/**
 * 
 */
UCLASS()
class SKILL_API UDestruction : public USkillBase
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

	// 적용할 Gameplay Effect 클래스 (파괴 로직이 담긴 GE)
	UPROPERTY(EditDefaultsOnly, Category = "Destruction")
	TSubclassOf<UGameplayEffect> DestructionEffectClass;

	// 이 태그를 가진 대상만 파괴 GE 적용 
	UPROPERTY(EditDefaultsOnly, Category = "Destruction")
	FGameplayTag DestructibleTag;

	// 적용할 Gameplay Effect 클래스 (피해 로직이 담긴 GE)
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 이 태그를 가진 대상만 파괴 GE 적용 
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	FGameplayTag EnemyTag;

	// 프리뷰 시각화용 액터 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Destruction")
	TSubclassOf<AActor> PreviewVisualizerClass;

	// -------------------------------------------------------------------
	// 내부 로직
	// -------------------------------------------------------------------
	UFUNCTION()
	void OnConfirmEventReceived(FGameplayEventData Payload);

	UPROPERTY()
	UPreviewTask* PreviewTask;
};
