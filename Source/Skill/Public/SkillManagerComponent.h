// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "SkillManagerComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UDA_Rune;

/**
 * 스킬 슬롯 구조체
 * 캐릭터가 장착한 스킬의 정보를 담는 구조체
 * Skill의 GA와 그 SpecHandle을 보관
 */
USTRUCT(BlueprintType)
struct FSkillSlot
{
	GENERATED_BODY()

	// 슬롯에 장착된 GA 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	TSubclassOf<UGameplayAbility> EquippedSkill;

	// ASC에서 받은 Ability Handle (GA 활성화/제거용)
	// GA의 SpecHandle은 GE의 SpecHandle과 달리 int32 ID를 가짐
	// GA Spec은 GA가 ASC에 장착될 때, 그 ASC가 TArray로 보관함
	// Handle을 통해 ASC가 보관 중인 GA Spec에 접근 가능
	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	FGameplayAbilitySpecHandle AbilityHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune")
	TArray<TObjectPtr<UDA_Rune>> EquippedRunes;

	// 기본 생성자
	FSkillSlot() : EquippedSkill(nullptr), AbilityHandle() {
		EquippedRunes.SetNum(3); // 룬 슬롯 3칸 확보
	}

	// TODO: 추후 필요하면 구현 예정
	// // 슬롯 이름 (UI용)
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	// FName SlotName;

	// // 슬롯 활성화 여부
	// UPROPERTY(BlueprintReadOnly, Category = "Skill")
	// bool bIsActive = true;

	// // 입력 바인딩 ID (옵션)
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	// int32 InputID = 0;
};

/**
 * 스킬 매니저 컴포넌트
 * 캐릭터의 스킬 장착/해제/활성화를 관리하는 컴포넌트
 */
// ClassGroup: 'Add Compnent' 메뉴에서 보이는 그룹 지정
// meta = (BlueprintSpawnableComponent): BP 에디터에서 이 컴포넌트를 추가 가능
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SKILL_API USkillManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	USkillManagerComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 플레이어의 ASC를 가져오는 함수
	// 캐릭터의 BeginPlay에서 호출되어야 함
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	void SkillManagerInitialize(UAbilitySystemComponent* InAbilitySystemComponent);

	// 특정 슬롯에 스킬을 장착하는 함수
	// @param SlotIndex: 장착할 슬롯의 인덱스 (0부터 시작)
	// @param SkillClass: 장착할 스킬 GA의 클래스
	// @return 성공 여부
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	bool EquipSkill(int32 SlotIndex, TSubclassOf<UGameplayAbility> SkillClass);

	// 특정 슬롯의 스킬을 해제하는 함수
	// @param SlotIndex: 해제할 슬롯의 인덱스
	// @return 성공 여부
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	bool UnequipSkill(int32 SlotIndex);

	// 특정 슬롯의 스킬을 활성화하는 함수
	// @param SlotIndex: 활성화할 슬롯의 인덱스
	// @return 성공 여부

	// 현재 장착된 모든 스킬 정보를 반환하는 함수
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	TArray<FSkillSlot> GetEquippedSkills() const;

	// 룬 장착 함수
	// @param SlotIndex: 스킬 슬롯 인덱스
	// @param RuneSlotIndex: 룬 슬롯 인덱스 (0~2)
	// @param RuneData: 장착할 룬 데이터 에셋
	// @return 성공 여부
	UFUNCTION(BlueprintCallable, Category = "Skill Manager|Rune")
	bool EquipRune(int32 SlotIndex, int32 RuneSlotIndex, UDA_Rune* RuneData);

	// 룬 장착 해제 함수
	// @param SlotIndex: 스킬 슬롯 인덱스
	// @param RuneSlotIndex: 해제할 룬 슬롯 인덱스 (0~2)
	// @return 성공 여부
	UFUNCTION(BlueprintCallable, Category = "Skill Manager|Rune")
	bool UnequipRune(int32 SlotIndex, int32 RuneSlotIndex);


	// 해당 슬롯의 '피해량(Red)' 룬 합계 반환 (예: 1.5 = 150%)
	UFUNCTION(BlueprintPure, Category = "Skill Manager|Calculation")
	float GetTotalDamageMultiplier(int32 SlotIndex) const;

	// 해당 슬롯의 '쿨타임(Yellow)' 룬 합계 반환
	UFUNCTION(BlueprintPure, Category = "Skill Manager|Calculation")
	float GetTotalCooldownReduction(int32 SlotIndex) const;

	// 해당 슬롯의 '범위(Orange)' 룬 합계 반환
	UFUNCTION(BlueprintPure, Category = "Skill Manager|Calculation")
	float GetTotalRangeMultiplier(int32 SlotIndex) const;

protected:
	// ASC 참조 (소유하지 않고 캐릭터로부터 받아서 사용)
	UPROPERTY(BlueprintReadOnly, Category = "Skill Manager")
	TObjectPtr<UAbilitySystemComponent> CachedAbilitySystemComponent;

	// 스킬 슬롯 배열
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Manager")
	TArray<FSkillSlot> SkillSlots;

	// 슬롯 인덱스가 유효한지 검사하는 헬퍼 함수
	bool IsValidSlotIndex(int32 SlotIndex) const;
};
