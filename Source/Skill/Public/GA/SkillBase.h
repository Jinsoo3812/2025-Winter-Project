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

	// -----------------------------------------------------------------------------
	// 스킬 GA의 시전, 취소 등 생명주기
	// -----------------------------------------------------------------------------

	/*
	* Gameplay Ability가 활성화될 때 호출되는 함수
	*/
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/*
	* 스킬이 시전된 후 다시 눌렸을 때 호출되는 함수
	*/
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	// -----------------------------------------------------------------------------
	// 시전자, GA 본인에게 태그 부착되는 태그 관리
	// -----------------------------------------------------------------------------

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

	// 내 어빌리티 스펙(Spec)에 동적으로 태그를 추가하는 함수
	void AddAbilityTag(const FGameplayTag& Tag);

	// 내 어빌리티 스펙(Spec)에서 동적 태그를 제거하는 함수
	void RemoveAbilityTag(const FGameplayTag& Tag);

	// 현재 나에게 특정 태그가 있는지 확인
	bool HasAbilityTag(const FGameplayTag& Tag) const;

	// -----------------------------------------------------------------------------
	// 캐싱 데이터
	// -----------------------------------------------------------------------------

	/* BlockManagerSubsystem 캐싱 */
	IBlockSystemInterface* BlockSystem;
};
