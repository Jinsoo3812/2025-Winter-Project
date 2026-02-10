// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "GameplayTagContainer.h"
#include "MMC_RuneCalculator.generated.h"

/**
 * 룬 태그 개수에 따라 수치를 증폭시키는 범용 계산기
 * 사용법: GE의 Modifier에서 Magnitude Calculation Class로 이 클래스를 지정
 */
UCLASS()
class SKILL_API UMMC_RuneCalculator : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()
	
public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

protected:
	// 이 MMC가 감지할 룬의 종류
	UPROPERTY(EditDefaultsOnly, Category = "Rune Config")
	FGameplayTag TargetRuneTypeTag;

	// 룬 개수에 따른 배율을 정의한 커브 테이블 (Key: 0, 1, 2, 3...)
	UPROPERTY(EditDefaultsOnly, Category = "Rune Config")
	FCurveTableRowHandle RuneCurveInfo;
};
