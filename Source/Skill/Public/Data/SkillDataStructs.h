// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayAbilitySpec.h"
#include "SkillDataStructs.generated.h"

class UGameplayAbility;

/**
 * 스킬 데이터 테이블 구조체
 * 각 스킬의 기본 수치를 정의
 */
USTRUCT(BlueprintType)
struct SKILL_API FSkillDataRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 스킬 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FText SkillName;

	// 스킬 클래스 (GA 블루프린트 또는 C++ 클래스)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	TSubclassOf<UGameplayAbility> SkillClass;

	// 기본 피해량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Stats")
	float BaseDamage = 10.0f;

	// 기본 쿨타임 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Stats")
	float BaseCooldown = 5.0f;

	// 기본 범위 계수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Stats")
	float BaseRange = 1.0f;

	// 스킬 설명 (UI용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FText Description;

	// 스킬 아이콘 (UI용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	TObjectPtr<UTexture2D> SkillIcon;
};
