// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "SkillManagerComponent.h"
#include "TestPlayerState.generated.h"

class UAbilitySystemComponent;
class USkillManagerComponent;
class UGameplayAbility;
class UPlayerAttributeSet;
/**
 * 멀티플레이 환경에서 플레이어의 GAS 및 각종 상태 정보를 관리하는 PlayerState
 * 플레이어가 죽었다 부활해도 유지되어야 하는 정보들을 담고 있다.
 */
UCLASS()
class WINTER2025_API ATestPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATestPlayerState();

	virtual void BeginPlay() override;

	// IAbilitySystemInterface 구현 및 GAS 접근자
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// SkillManager 접근자
	UFUNCTION(BlueprintCallable, Category = "GAS")
	USkillManagerComponent* GetSkillManager() const { return SkillManager; }

	// AttributeSet 접근자 (체력/마나 등의 수치 데이터)
	UFUNCTION(BlueprintCallable, Category = "GAS")
	UPlayerAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// DefaultRunes 접근자
	const TArray<FSkillSlot>& GetDefaultSkillSets() const { return DefaultSkillSets; }

protected:
	// PlayerState가 소유하는 AbilitySystemComponent
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// 스킬 매니저 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill System")
	TObjectPtr<USkillManagerComponent> SkillManager;

	// 플레이어의 체력, 최대체력 등의 수치를 관리하는 AttributeSet
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UPlayerAttributeSet> AttributeSet;

	// FSkillSlot 구조체를 사용하여 스킬과 해당 스킬의 룬을 한 번에 설정
	UPROPERTY(EditDefaultsOnly, Category = "Skill System")
	TArray<FSkillSlot> DefaultSkillSets;
};
