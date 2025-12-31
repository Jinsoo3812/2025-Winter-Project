// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Winter2025Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "TestCharacter.generated.h"

class UGameplayAbility;
class UInputAction;
class UInputMappingContext;
class USkillManagerComponent;

UCLASS()
class WINTER2025_API ATestCharacter : public AWinter2025Character, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ATestCharacter();

	// IAbilitySystemInterface 구현
	// 원본이 0을 반환하므로 override 필요
	// @return TestCharacter의 AbilitySystemComponent
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;

	// TestCharacter의 ASC
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// 스킬 매니저 컴포넌트 - 스킬 장착/해제/활성화 관리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill System")
	TObjectPtr<USkillManagerComponent> SkillManager;

	// 스킬 슬롯별 Input Action (슬롯 0, 1, 2)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Skill Slots")
	TObjectPtr<UInputAction> SkillSlot0InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Skill Slots")
	TObjectPtr<UInputAction> SkillSlot1InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Skill Slots")
	TObjectPtr<UInputAction> SkillSlot2InputAction;

	// Input Mapping Context 할당 변수
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;
};
