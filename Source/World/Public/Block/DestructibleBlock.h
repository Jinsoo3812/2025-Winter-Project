// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlockBase.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "DestructibleBlock.generated.h"

class UBlockAttributeSet;

/**
 * 
 */
UCLASS()
class WORLD_API ADestructibleBlock : public ABlockBase, public IAbilitySystemInterface
{
	GENERATED_BODY()

	// -------------------------------------------------------------
	// 초기화 및 기본 함수
	// -------------------------------------------------------------
public:
	ADestructibleBlock();

	// BossDragon이 SetLifeSpan을 사용하므로 일단..
	virtual void LifeSpanExpired() override;

protected:
	virtual void BeginPlay() override;

	// -------------------------------------------------------------
	// ASC
	// -------------------------------------------------------------
public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }

protected:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	UBlockAttributeSet* BlockAttributeSet;

	// -------------------------------------------------------------
	// 파괴
	// -------------------------------------------------------------
protected:
	// 자신을 파괴하는 함수
	void SelfDestroy() override;

	// AttributeSet의 OnDestroyed 델리게이트에 바인딩 될 함수
	void HandleDestroyed(AActor* InstigatorActor);
};
