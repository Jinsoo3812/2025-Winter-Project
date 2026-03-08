// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "BlockCommonTypes.h"
#include "GameplayAbilityTargetData_Blocks.generated.h"

/**
 * HISM 블록과 Actor 블록이 혼재된 FBlockReference 배열을
 * 최소한의 네트워크 대역폭으로 동기화하기 위한 커스텀 TargetData 구조체.
 */
USTRUCT(BlueprintType)
struct WINTER2025CORE_API FGameplayAbilityTargetData_Blocks : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	// 클라이언트가 수집한 대상 블록들
	UPROPERTY()
	TArray<FBlockReference> Blocks;

	// -------------------------------------------------------------------
	// FGameplayAbilityTargetData 필수 오버라이드
	// -------------------------------------------------------------------

	// 다형성을 위해 이 구조체의 타입 정보를 엔진에 반환
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FGameplayAbilityTargetData_Blocks::StaticStruct();
	}

	// 네트워크 전송 시 비트 단위로 데이터를 압축/해제하는 핵심 함수
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

// 언리얼 리플렉션 시스템에 커스텀 NetSerialize가 존재함을 알리는 Trait 매크로/템플릿 특수화
template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetData_Blocks> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetData_Blocks>
{
	enum
	{
		WithNetSerializer = true // 엔진이 기본 직렬화 대신 위에서 정의한 NetSerialize를 호출하게 만듦
	};
};