// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "PreviewTask.h"
#include "AbilitySystemComponent.h" 
#include "AbilitySystemBlueprintLibrary.h" 
#include "BlockCommonTypes.h"
#include "SkillGameplayTags.h"
#include "SkillComponent.h"
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

	// -----------------------------------------------------------------------------
	// 스킬 GA의 기본 스펙
	// -----------------------------------------------------------------------------
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Skill Stats")
	float BaseDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Skill Stats")
	float BaseCooldown = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Skill Stats")
	FSkillPreviewRange PreviewRange;

	// -------------------------------------------------------------------
	// Gameplay Tags
	// -------------------------------------------------------------------

	// 스킬 인스턴스에 붙을 상태 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Skill_State_Preview;

	// 블록 하이라이트용 태그 (범위 표시)
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Highlight_Range;

	// 블록 하이라이트용 태그 (마우스 커서)
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Highlight_Cursor;

	// 입력 이벤트: 좌클릭(확인)
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag Tag_Event_Confirm;

	// 아군(플레이어)은 밀어내기 위한 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag TeamAllyTag;

	// 적(다른 블록, 적)과의 충돌에서 폭발하기 위한 태그
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag TeamEnemyTag;

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

	virtual void StartPreview();

	// -----------------------------------------------------------------------------
	// 쿨타임
	// -----------------------------------------------------------------------------
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag CooldownTag;

	// 매 틱/프레임마다 태그 검사 시 메모리 재할당을 막기 위한 캐싱 컨테이너
	UPROPERTY(Transient)
	mutable FGameplayTagContainer TempCooldownTags;

	// CanActivateAbility에서 쿨타임 검사 시 사용할 태그 컨테이너 반환
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// -----------------------------------------------------------------------------
	// 시전자, GA 본인에게 태그 부착되는 태그 관리
	// -----------------------------------------------------------------------------

	/*
	* 시전자(Owner)에게 Gameplay Tag를 추가
	* LooseTag는 Replication 정책에 따라 동기화되지 않을 수 있음
	* @param TagToAdd 추가할 Gameplay Tag
	*/
	void AddGameplayTagToOwner(const FGameplayTag& TagToAdd);

	/*
	* 시전자(Owner)에게서 Gameplay Tag를 제거
	* LooseTag는 Replication 정책에 따라 동기화되지 않을 수 있음
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
	// GE 적용 관련 유틸리티
	// -----------------------------------------------------------------------------
protected:
	// 다양한 포인터 타입에서 AActor*를 꺼내는 로직
	FORCEINLINE AActor* ExtractActor(AActor* Ptr) { return Ptr; }
	FORCEINLINE AActor* ExtractActor(const TWeakObjectPtr<AActor>& Ptr) { return Ptr.Get(); }
	FORCEINLINE AActor* ExtractActor(const TObjectPtr<AActor>& Ptr) { return Ptr.Get(); }
	// FBlockReference 에서도 AActor* 추출
	FORCEINLINE AActor* ExtractActor(const FBlockReference& Ref) { return Cast<AActor>(Ref.TargetObject.Get()); }

	/*
	* 다수의 액터에게 동일한 GE를 적용하는 헬퍼 함수
	* @param Targets GE를 적용할 대상 액터들의 포인터 배열
	* @param SpecHandle 적용할 GE의 스펙 핸들
	* @return 적용에 성공한 액터 수
	*/
	template <typename T> // Template이라서 헤더에 구현
	int32 ApplyGameplayEffectToTargets(const TArray<T>& Targets, const FGameplayEffectSpecHandle& SpecHandle)
	{
		if (Targets.IsEmpty() || !SpecHandle.IsValid()) {
			return 0;
		}

		UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
		if (!SourceASC) return 0;

		int32 ApplyCount = 0;

		for (const T& Item : Targets)
		{
			AActor* TargetActor = ExtractActor(Item);
			if (!TargetActor) continue;

			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
			if (!TargetASC) continue;

			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
			ApplyCount++;
		}

		return ApplyCount;
	}

	// SkillComponent로 부터 현재 슬롯의 룬 배율을 가져오는 함수
	float GetRuneMultiplier(ERuneType RuneType) const;


	// -----------------------------------------------------------------------------
	// 캐싱 데이터
	// -----------------------------------------------------------------------------
public:
	// Ability Task에게 BlockSystemInterface를 제공
	IBlockSystemInterface* GetBlockSystem() const { return BlockSystem; }

protected:
	// BlockManagerSubsystem 캐싱
	IBlockSystemInterface* BlockSystem;

	USkillComponent* SkillComp;
};
