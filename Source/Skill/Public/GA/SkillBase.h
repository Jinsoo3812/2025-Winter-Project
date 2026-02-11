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
	* @param TargetActors      적용 대상 액터 배열
	* @param EffectClass       적용할 GE 클래스
	* @param RequiredTag       이 태그를 가진 대상에게만 적용 (EmptyTag면 검사 건너뜀)
	* @param Level             GE 레벨
	* @return                  성공적으로 적용된 횟수
	*/
	template <typename T> // Template이라서 헤더에 구현
	int32 ApplyGameplayEffectToTargets(const TArray<T>& Targets,
		TSubclassOf<UGameplayEffect> EffectClass, float Level = 1.0f)
	{
		if (Targets.IsEmpty() || !EffectClass) {
			UE_LOG(LogTemp, Warning, TEXT("SkillBase: ApplyGameplayEffectToTargets - Invalid targets or effect class"));
			return 0;
		}

		UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
		if (!SourceASC) {
			UE_LOG(LogTemp, Warning, TEXT("SkillBase: ApplyGameplayEffectToTargets - SourceASC is null"));
			return 0;
		}

		// Context 생성 
		FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
		ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());

		// 시전 GA 주입
		ContextHandle.SetAbility(this);

		// Spec 생성
		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(EffectClass, Level, ContextHandle);
		if (!SpecHandle.IsValid()) return 0;

		// 룬 배율 계산
		float RuneMultiplier = 1.0f;

		// SkillComponent 찾기
		if (AActor* OwnerActor = GetOwningActorFromActorInfo())
		{
			if (USkillComponent* SkillComp = OwnerActor->FindComponentByClass<USkillComponent>())
			{
				// 2. 내 슬롯 태그 찾기 ("Skill.Slot" 포함 여부 확인)
				if (const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec())
				{
					for (const FGameplayTag& Tag : Spec->DynamicAbilityTags)
					{
						if (Tag.MatchesTag(TAG_Skill_Slot))
						{
							// 3. 배율 요청 (데미지니까 Red)
							RuneMultiplier = SkillComp->GetTotalRuneMultiplier(Tag, ERuneType::Red);
							break;
						}
					}
				}
			}
		}

		// 스킬 기본 피해량 SetByCaller로 주입
		SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Damage, BaseDamage);
		// 룬 배율 SetByCaller로 주입
		SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_RuneMultiplier, RuneMultiplier);

		int32 ApplyCount = 0;

		// 템플릿 루프: 무엇이 들어오든 ExtractActor가 처리함
		for (const T& Item : Targets)
		{
			// 헬퍼 함수를 통해 실제 액터 포인터 추출
			AActor* TargetActor = ExtractActor(Item);

			if (!TargetActor) continue;

			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
			if (!TargetASC) continue;

			// 적용
			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
			UE_LOG(LogTemp, Log, TEXT("SkillBase: Applied GE %s to %s"),
				*EffectClass->GetName(),
				*TargetActor->GetName()
			);
			ApplyCount++;
		}

		return ApplyCount;
	}


	// -----------------------------------------------------------------------------
	// 캐싱 데이터
	// -----------------------------------------------------------------------------

	/* BlockManagerSubsystem 캐싱 */
	IBlockSystemInterface* BlockSystem;
};
