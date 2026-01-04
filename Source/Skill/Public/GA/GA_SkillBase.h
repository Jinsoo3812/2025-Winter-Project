// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "NativeGameplayTags.h"
#include "GA_SkillBase.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Damage);   // 데미지 전달용
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Cooldown); // 쿨타임 전달용

/**
 * 모든 액티브 스킬의 부모 클래스
 * 룬 처리 로직 포함
 */
UCLASS()
class SKILL_API UGA_SkillBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_SkillBase();

	// 쿨타임 적용 로직을 오버라이드 (노랑 룬 적용을 위해)
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	// 스킬의 기본 스펙 (에디터 설정용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats")
	float BaseDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats")
	float BaseCooldown = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats")
	float BaseRange = 1000.0f;

	// 데미지 적용을 위한 GE 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Effects")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// 빨강 룬이 적용된 최종 데미지 값을 계산해서 반환
	UFUNCTION(BlueprintCallable, Category = "Skill|Calculation")
	float GetRuneModifiedDamage() const;

	// 주황 룬이 적용된 최종 범위 값을 계산해서 반환
	UFUNCTION(BlueprintCallable, Category = "Skill|Calculation")
	float GetRuneModifiedRange() const;

	// 노랑 룬이 적용된 최종 쿨타임 값을 계산해서 반환
	UFUNCTION(BlueprintCallable, Category = "Skill|Calculation")
	float GetRuneModifiedCooldown() const;

	// 데미지 GE Spec을 만들고 룬 수치를 주입해서 반환하는 함수
	FGameplayEffectSpecHandle MakeRuneDamageEffectSpec(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;
};