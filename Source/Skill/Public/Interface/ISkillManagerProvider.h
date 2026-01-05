// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ISkillManagerProvider.generated.h"

class USkillManagerComponent;

// 언리얼 리플렉션 시스템용 더미 클래스
UINTERFACE(MinimalAPI, Blueprintable)
class USkillManagerProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * SkillManagerComponent에 대한 접근을 제공하는 인터페이스
 * TestCharacter 등 SkillManager를 캐싱하고 있는 클래스가 구현
 * Skill 모듈이 Winter2025 모듈에 의존하지 않고도 SkillManager에 접근 가능
 */
class SKILL_API ISkillManagerProvider
{
	GENERATED_BODY()

public:
	// SkillManagerComponent를 반환하는 순수 가상 함수
	virtual USkillManagerComponent* GetSkillManager() const = 0;
};
