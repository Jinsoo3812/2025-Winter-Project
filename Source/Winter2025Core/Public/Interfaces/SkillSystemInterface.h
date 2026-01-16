// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SkillSystemInterface.generated.h"

class USkillManagerComponent;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class USkillSystemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WINTER2025CORE_API ISkillSystemInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// 스킬 매니저 컴포넌트 반환 함수
	virtual USkillManagerComponent* GetSkillManager() const = 0;

	// 스킬 매니저 초기화 함수
	virtual void InitializeSkills() = 0;
};
