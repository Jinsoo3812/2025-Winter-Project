// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlockBase.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "DestructibleBlock.generated.h"

/**
 * 
 */
UCLASS()
class WORLD_API ADestructibleBlock : public ABlockBase, public IAbilitySystemInterface
{
	// ASC를 갖는 클래스는 IAbilitySystemInterface를 상속
	GENERATED_BODY()
public:
	ADestructibleBlock();

	// ASC를 반환. ASC는 Game Ability, Game Effect 등을 관리
	// 순수 가상 함수이므로 반드시 구현
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;

	// Ability System Component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// 파괴 트리거 태그 (이 태그를 가진 GE를 받으면 파괴됨)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FGameplayTag DestructionTag;

	// 자신을 파괴하는 함수
	void SelfDestroy();

	// GE를 적용받았을 때 호출할 콜백 함수. 이 곳에서 태그를 검사한다.
	// Target: GE가 적용된 대상의 ASC (보통 본인)
	// SpecApplied: 적용된 GE의 스펙 (Attributes, Tags 등 포함)
	// ActiveHandle: 적용된 GE의 핸들 (GE의 식별자이며 GE가 활성화된 동안 유효)
	UFUNCTION()
	void OnGameplayEffectApplied(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle);
};
