// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IAttributeSetProvider.generated.h"

class UAttributeSet;

// 언리얼 리플렉션 시스템을 위한 클래스
UINTERFACE(MinimalAPI, Blueprintable)
class UAttributeSetProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * AttributeSet을 제공하는 인터페이스
 * TestCharacter 등 PlayerState의 AttributeSet을 캐싱하고 있는 클래스가 구현
 * Skill 모듈이 Winter2025 모듈에 의존하지 않고도 AttributeSet에 접근 가능
 */
class SKILL_API IAttributeSetProvider
{
	GENERATED_BODY()

public:
	// AttributeSet을 반환하는 순수 가상 함수
	virtual UAttributeSet* GetAttributeSet() const = 0;
};
