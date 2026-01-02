// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "TestPlayerState.generated.h"

class UAbilitySystemComponent;
class USkillManagerComponent;
class UGameplayAbility;

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

	// IAbilitySystemInterface 구현 및 GAS 접근자
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// SkillManager 접근자
	UFUNCTION(BlueprintCallable, Category = "GAS")
	USkillManagerComponent* GetSkillManager() const { return SkillManager; }

	// DefaultSkills 접근자
	const TArray<TSubclassOf<UGameplayAbility>>& GetDefaultSkills() const { return DefaultSkills; }

protected:
	// PlayerState가 소유하는 AbilitySystemComponent
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// 스킬 매니저 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill System")
	TObjectPtr<USkillManagerComponent> SkillManager;

	// 스테이지 시작 시 자동으로 장착할 기본 스킬 목록
	UPROPERTY(EditDefaultsOnly, Category = "Skill System")
	TArray<TSubclassOf<UGameplayAbility>> DefaultSkills;
};
