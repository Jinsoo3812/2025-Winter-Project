// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "NativeGameplayTags.h"
#include "GA_SkillBase.generated.h"

class USkillManagerComponent;

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill); // 스킬임을 표시하는 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_Casting); // 스킬 시전 중임을 시전자에게 부여하는 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Damage);   // 데미지 태그용
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Cooldown); // 쿨타임 태그용

/**
 * 모든 액티브 스킬의 부모 클래스
 * 룬 적용 로직 포함
 */
UCLASS()
class SKILL_API UGA_SkillBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_SkillBase();

protected:
	// 스킬의 기본 스펙 (에디터 설정용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats")
	float BaseDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats")
	float BaseCooldown = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats")
	float BaseRange = 1.0f;

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

	// 데미지 GE Spec을 생성할 때 수치를 주입해서 반환하는 함수
	FGameplayEffectSpecHandle MakeRuneDamageEffectSpec(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	// GA 종료 시 호출되는 함수
	// State.Busy 태그 제거 수행
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// 쿨타임 적용 로직을 오버라이드 (노랑 룬 적용을 위해)
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// 실제 스킬이 발동될 때 호출되는 함수
	// 프리뷰 단계가 아닌 실제 시작단계에서 이 함수를 호출한다.
	void NotifySkillCastStarted();

private:
	// SkillManager를 가져오는 헬퍼 함수
	// 성능 최적화를 위해 캐싱된 값이 있으면 재사용
	USkillManagerComponent* GetSkillManagerFromAvatar() const;

	// 캐싱된 SkillManager (성능 최적화용)
	// mutable: const 함수에서도 수정 가능
	mutable TWeakObjectPtr<USkillManagerComponent> CachedSkillManager;
};