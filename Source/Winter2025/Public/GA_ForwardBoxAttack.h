// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_ForwardBoxAttack.generated.h"

class UGameplayEffect;

UCLASS()
class WINTER2025_API UGA_ForwardBoxAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_ForwardBoxAttack();

	// GA 발동 시 호출되는 함수
	// Handle: GA 식별자
	// ActorInfo: GA를 소유한 액터(Owner, Avatear)의 정보
	// ActivationInfo: GA 발동에 대한 추가 정보 (예측, 서버 전용, 서버 대기 등)
	// TriggerEventData: GA가 GameplayEvent에 의해 발동된 경우의 이벤트 데이터. 입력/직접 호출의 경우 nullptr
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	float TraceDistance = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayEffect> TestGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	float DebugDrawDuration = 2.0f;
};
