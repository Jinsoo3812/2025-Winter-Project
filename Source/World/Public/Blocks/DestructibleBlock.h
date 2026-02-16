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
class WORLD_API ADestructibleBlock : public ABlockBase
{
	GENERATED_BODY()

	// -------------------------------------------------------------
	// 초기화 및 기본 함수
	// -------------------------------------------------------------
public:
	// BossDragon이 SetLifeSpan을 사용하므로 일단..
	virtual void LifeSpanExpired() override;

protected:
	virtual void BeginPlay() override;

	// -------------------------------------------------------------
	// 파괴
	// -------------------------------------------------------------
protected:
	// 자신을 파괴하는 함수
	void SelfDestroy() override;

	// AttributeSet의 OnDestroyed 델리게이트에 바인딩 될 함수
	virtual void OnDestroyed(AActor* InstigatorActor) override;
};
