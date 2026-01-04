// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "DA_Rune.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Rune_Red);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Rune_Yellow);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Rune_Orange);

/**
 * Rune Data Asset
 */
UENUM(BlueprintType)
enum class ERuneType : uint8
{
    // 데미지 증가량
    Red     UMETA(DisplayName = "Damage"),

	// 쿨타임 감소량
    Yellow  UMETA(DisplayName = "Cooldown"),

    // 범위 증가량
    Orange  UMETA(DisplayName = "Range")
};

UCLASS()
class SKILL_API UDA_Rune : public UDataAsset
{
	GENERATED_BODY()
public:
	// 룬의 타입을 나타내는 태그
    UPROPERTY(EditDefaultsOnly, Category = "Rune")
    FGameplayTag RuneTag;

	// 적용할 수치 (ex. 1.1 = 10% 증가)
    UPROPERTY(EditDefaultsOnly, Category = "Rune")
    float RuneValue;
};
