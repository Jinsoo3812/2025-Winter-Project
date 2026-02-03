// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "PreviewTask.h"
#include "SkillBase.generated.h"

class IBlockSystemInterface;

/**
 * 플레이어가 사용하는 모든 Gameplay Ability의 부모 클래스
 */
UCLASS()
class SKILL_API USkillBase : public UGameplayAbility
{
	GENERATED_BODY()
public:
	USkillBase();

	/*
	* Ability Task에게 BlockSystemInterface를 제공
	*/
	IBlockSystemInterface* GetBlockSystem() const { return BlockSystem; }

protected:
	// -----------------------------------------------------------------------------
	// 스킬 GA의 기본 스펙
	// -----------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "Skill Stats")
	float BaseDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Skill Stats")
	float BaseCooldown = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Skill Stats")
	float BaseRange = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Skill Stats")
	float RangeXY = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Skill Stats")
	float RangeZ = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Skill Stats")
	FSkillPreviewRange PreviewRange;

	/*
	* Gameplay Ability가 활성화될 때 호출되는 함수
	*/
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/*
	* 시전자(Owner)에게 Gameplay Tag를 추가
	* LooseTag는 Replication 저책에 따라 동기화되지 않을 수 있음
	* @param TagToAdd 추가할 Gameplay Tag
	*/
	void AddGameplayTagToOwner(const FGameplayTag& TagToAdd);

	/*
	* 시전자(Owner)에게서 Gameplay Tag를 제거
	* LooseTag는 Replication 저책에 따라 동기화되지 않을 수 있음
	* @param TagToRemove 제거할 Gameplay Tag
	*/
	void RemoveGameplayTagFromOwner(const FGameplayTag& TagToRemove);

	/* BlockManagerSubsystem 캐싱 */
	IBlockSystemInterface* BlockSystem;
};
