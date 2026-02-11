// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NativeGameplayTags.h"
#include "DA_Rune.generated.h"

class UGameplayAbility;

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Rune_Red);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Rune_Yellow);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Rune_Blue);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Rune_Green);

/**
 * Rune Data Asset
 */
UENUM(BlueprintType)
enum class ERuneType : uint8
{
    // 데미지 증폭룬
    Red     UMETA(DisplayName = "Damage"),

	// 쿨타임 감소룬
    Yellow  UMETA(DisplayName = "Cooldown"),

    // 범위 증폭룬
    Blue  UMETA(DisplayName = "Range"),

	// 스킬 변환룬
	Green UMETA(DisplayName = "Skill Transform")
};

UCLASS()
class SKILL_API UDA_Rune : public UDataAsset
{
	GENERATED_BODY()
public:
	// 룬 타입
	UPROPERTY(EditDefaultsOnly, Category = "Rune")
	ERuneType RuneType = ERuneType::Red;

	// [초록룬 전용] 변환 대상이 되는 원본 스킬 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Rune|Green", meta = (EditCondition = "RuneType == ERuneType::Green", EditConditionHides))
	TSubclassOf<UGameplayAbility> OriginalSkillClass;

	// [초록룬 전용] 변환 후 스킬 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Rune|Green", meta = (EditCondition = "RuneType == ERuneType::Green", EditConditionHides))
	TSubclassOf<UGameplayAbility> ReplacementSkillClass;
};
