#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillSystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "SkillComponent.generated.h"

class UGameplayAbility;
class UAbilitySystemComponent;
class UDA_Rune;

/**
 * 룬 슬롯 구조체
 * 개별 룬 슬롯의 상태를 관리
 */
USTRUCT(BlueprintType)
struct FRuneSlot
{
	GENERATED_BODY()

	// 장착된 룬 에셋
	UPROPERTY(EditDefaultsOnly, Category = "Rune")
	TObjectPtr<UDA_Rune> RuneAsset;

	FRuneSlot() : RuneAsset(nullptr) {}
};

/**
 * 스킬 슬롯 구조체
 * 캐릭터가 장착한 스킬의 정보를 담는 구조체
 * Skill의 GA와 그 SpecHandle을 보관
 */
USTRUCT(BlueprintType)
struct FSkillSlot
{
	GENERATED_BODY()

	// 이 슬롯이 어떤 입력 태그에 반응하는지 명시 (ex: Input.Skill.Slot1)
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FGameplayTag SlotTag;

	// 슬롯에 장착된 GA 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Skill")
	TSubclassOf<UGameplayAbility> EquippedSkill;

	// ASC에서 받은 Ability Handle (GA 활성화/제거용)
	// GA의 SpecHandle은 GE의 SpecHandle과 달리 int32 ID를 가짐
	// GA Spec은 GA가 ASC에 장착될 때, 그 ASC가 TArray로 보관함
	// Handle을 통해 ASC가 보관 중인 GA Spec에 접근 가능
	UPROPERTY()
	FGameplayAbilitySpecHandle AbilityHandle;

	// 장착된 룬 슬롯들
	UPROPERTY(EditDefaultsOnly, Category = "Rune")
	TArray<FRuneSlot> RuneSlots;

	UPROPERTY(EditDefaultsOnly, Category = "Rune")
	int32 MaxRuneSlots = 3;

	// 장착된 초록 룬 (캐싱) - nullptr이면 초록 룬 없음
	UPROPERTY()
	TObjectPtr<UDA_Rune> EquippedGreenRune;

	// 기본 생성자
	FSkillSlot() : EquippedSkill(nullptr), AbilityHandle(), EquippedGreenRune(nullptr) {
		RuneSlots.SetNum(MaxRuneSlots ? MaxRuneSlots : 3); // 룬 슬롯 확보
	}

	// 초록 룬 캐시 업데이트 (룬 장착/해제 시 호출)
	void UpdateGreenRuneCache();
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class USkillComponent : public UActorComponent, public ISkillSystemInterface
{
	GENERATED_BODY()

public:
	USkillComponent();

	// 외부(PlayerState)에서 ASC가 준비되었음을 알릴 때 호출
	void InitializeSkillSystem(UAbilitySystemComponent* InASC) override;

	// 로비나 인게임에서 스킬을 교체할 때 호출
	UFUNCTION(BlueprintCallable, Category = "Skill")
	void EquipSkill(FGameplayTag SlotTag, TSubclassOf<UGameplayAbility> AbilityClass);

	// 리플리케이션 규칙 정의 함수 (SkillSlots  동기화)
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -----------------
	// 룬 관련 함수들
	// -----------------

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

	// 룬 ID(RowName)를 통해 데이터 테이블에서 룬을 찾아 장착하는 함수
	// @param SlotIndex: 스킬 슬롯 인덱스
	// @param RuneSlotIndex: 룬 슬롯 인덱스
	// @param RuneID: 데이터 테이블의 행 이름 (RowName)
	UFUNCTION(BlueprintCallable, Category = "Skill Manager|Rune")
	bool EquipRuneByID(int32 SlotIndex, int32 RuneSlotIndex, FName RuneID);

	// 해당 슬롯의 '피해량(Red)' 룬 합계 반환 (예: 1.5 = 150%)
	UFUNCTION(BlueprintPure, Category = "Skill Manager|Calculation")
	float GetTotalDamageMultiplier(int32 SlotIndex) const;

	// 해당 슬롯의 '쿨타임(Yellow)' 룬 합계 반환
	UFUNCTION(BlueprintPure, Category = "Skill Manager|Calculation")
	float GetTotalCooldownReduction(int32 SlotIndex) const;

	// 해당 슬롯의 '범위(Blue)' 룬 합계 반환
	UFUNCTION(BlueprintPure, Category = "Skill Manager|Calculation")
	float GetTotalRangeMultiplier(int32 SlotIndex) const;

protected:
	// 캐시된 ASC 포인터
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	// 스킬 슬롯 배열
	UPROPERTY(EditDefaultsOnly, Category = "Skill Manager", Replicated)
	TArray<FSkillSlot> SkillSlots;

	// 룬 데이터베이스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Manager")
	TObjectPtr<UDataTable> RuneDataTable;

	// 현재 장착된 스킬 핸들 관리 (SlotTag -> SpecHandle)
	TMap<FGameplayTag, FGameplayAbilitySpecHandle> ActiveSkillHandles;

	// BP Class Defaults에서 설정하는 초기 스킬셋
	UPROPERTY(EditDefaultsOnly, Category = "Skill")
	TArray<FSkillSlot> DefaultSkillSets;

	// 실제 어빌리티 부여 로직
	void GiveAbility(FGameplayTag SlotTag, TSubclassOf<UGameplayAbility> AbilityClass);

	// 슬롯 인덱스가 유효한지 검사하는 헬퍼 함수
	bool IsValidSlotIndex(int32 SlotIndex) const;
};