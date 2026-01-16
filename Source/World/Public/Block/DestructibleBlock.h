// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlockBase.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "DestructibleBlock.generated.h"

class UBlockAttributeSet;

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

	// 자신을 파괴하는 함수
	virtual void SelfDestroy();

protected:
	virtual void BeginPlay() override;

	// Ability System Component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	UBlockAttributeSet* BlockAttributeSet;

	// AttributeSet의 OnDestroyed 델리게이트에 바인딩 될 함수
	void HandleDestroyed(AActor* InstigatorActor);
};
