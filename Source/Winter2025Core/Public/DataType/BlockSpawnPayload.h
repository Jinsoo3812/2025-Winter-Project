// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "BlockSpawnPayload.generated.h"

UCLASS()
class WINTER2025CORE_API UBlockSpawnPayload : public UObject
{
	GENERATED_BODY()
	
	// ---------------------------------------------------------------------
	// 공통 데이터
	// ---------------------------------------------------------------------
};

UCLASS()
class WINTER2025CORE_API UBarrierSpawnPayload : public UBlockSpawnPayload
{
	GENERATED_BODY()

public:
	// 발사 속도
	UPROPERTY()
	float LaunchSpeed;

	UPROPERTY()
	// 자폭 수명
	float MaxLifeTime;

	UPROPERTY()
	// 아군(플레이어 등)을 식별하는 태그
	FGameplayTag TeamAllyTag;

	UPROPERTY()
	// 적(다른 블록, 적 등)을 식별하는 태그
	FGameplayTag TeamEnemyTag;

	UPROPERTY()
	// 아군 넉백 힘
	float AllyKnockbackStrength = 800.0f;

	UPROPERTY()
	// 폭발 시 적용할 데미지 이펙트 클래스
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};
