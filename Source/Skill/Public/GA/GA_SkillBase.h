// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "NativeGameplayTags.h"
#include "GA_SkillBase.generated.h"

class USkillManagerComponent;
class IBlockSpawnInterface;

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Player);

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
	// -----------------------------------------------------------------------------
	// 스킬 GA의 기본 스펙
	// -----------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats")
	float BaseDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats")
	float BaseCooldown = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats")
	float BaseRange = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats")
	float RangeXY = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats")
	float RangeZ = 200.0f;

	// -----------------------------------------------------------------------------
	// 프리뷰 및 범위 표시 관련
	// -----------------------------------------------------------------------------

	// 블록 스폰을 주문할 WorldSubsystem
	IBlockSpawnInterface* BlockSpawner;

	/* 프리뷰로 표시할 블록 클래스 */
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	TSubclassOf<AActor> PreviewBlockClass;

	/* 프리뷰로 생성된 블록 인스턴스 */	
	UPROPERTY()
	TObjectPtr<AActor> PreviewBlock;

	/* 프리뷰 중이거나 하이라이트 효과가 적용된 블록들을 관리하는 배열 */
	TArray<AActor*> PreviewBlocks;

	/*
	* 스킬 사용 범위 표시에 들어오는 블록들을 찾아내는 헬퍼 함수
	* 스킬 사용 범위를 찾는 로직이 다른 스킬들을 위한 가상함수
	*/
	virtual void FindBlocksInRange(TArray<AActor*>& OutActors);

	/*
	* GameplayTag을 이용해 블록들을 하이라이트를 변경하는 함수
	* @param Actors 하이라이트를 변경할 블록들의 배열
	* @param EventTag 하이라이트 변경에 사용할 GameplayTag
	*/
	void BatchHighlightBlocks(TArray<AActor*>& Actors, FGameplayTag EventTag);

	/*
	* 배열 초기화, 블록 찾기, 하이라이트 적용까지 한 번에 처리하는 헬퍼 함수
	* @param Actors 하이라이트를 변경할 블록들의 배열
	* @param EventTag 하이라이트 변경에 사용할 GameplayTag
	*/
	void HighlightBlocks(TArray<AActor*>& Actors, FGameplayTag EventTag);

	/*
	* 블록들에 적용된 하이라이트를 제거하는 함수
	* @param Actors 하이라이트를 제거할 블록들의 배열
	*/
	void ClearHighlights(TArray<AActor*>& Actors);

	// -----------------------------------------------------------------------------
	// Rune 관련
	// -----------------------------------------------------------------------------

	/*
	* SkillManager를 가져오는 헬퍼 함수
	* 성능 최적화를 위해 캐싱된 값이 있으면 재사용
	*/
	USkillManagerComponent* GetSkillManagerFromAvatar() const;

	/*
	* 캐싱된 SkillManager (성능 최적화용)
	* mutable: const 함수에서도 수정 가능
	*/
	mutable TWeakObjectPtr<USkillManagerComponent> CachedSkillManager;

	/* 데미지 적용을 위한 GE 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Effects")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/* 쿨타임 구분을 위한 Gameplay Tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Stats|Cooldown")
	FGameplayTagContainer UniqueCooldownTags;

	/* 빨강 룬이 적용된 최종 데미지 값을 계산해서 반환 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Calculation")
	float GetRuneModifiedDamage() const;

	/* 주황 룬이 적용된 최종 범위 값을 계산해서 반환 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Calculation")
	float GetRuneModifiedRange() const;

	/* 노랑 룬이 적용된 최종 쿨타임 값을 계산해서 반환 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Calculation")
	float GetRuneModifiedCooldown() const;

	/* 데미지 GE Spec을 생성할 때 수치를 주입해서 반환하는 함수 */
	FGameplayEffectSpecHandle MakeRuneDamageEffectSpec(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	// -----------------------------------------------------------------------------
	// Gameplay Effect 및 쿨타임 관련
	// -----------------------------------------------------------------------------

	/*
	* GA 종료 시 호출되는 함수
	* State.Busy 태그 제거 수행
	*/
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/* 쿨타임 적용 로직을 오버라이드(노랑 룬 적용을 위해) */
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual const FGameplayTagContainer* GetCooldownTags() const override {
		return &UniqueCooldownTags;
	}

	/* 실제 스킬이 발동될 때 호출되는 함수 */
	void NotifySkillCastStarted();

	/* 실제 스킬이 종료될 때 호출되는 함수 */
	void NotifySkillCastFinished();
};
