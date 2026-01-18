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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune")
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

	// 이 슬롯이 어떤 입력 태그에 반응하는지 명시
	// BP에서 설정: "Input.Ability.Slot1", "Input.Ability.Slot2" 등
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FGameplayTag SlotTag;

	// 슬롯에 장착된 GA 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	TSubclassOf<UGameplayAbility> EquippedSkill;

	// ASC에서 받은 Ability Handle (GA 활성화/제거용)
	// GA의 SpecHandle은 GE의 SpecHandle과 달리 int32 ID를 가짐
	// GA Spec은 GA가 ASC에 장착될 때, 그 ASC가 TArray로 보관함
	// Handle을 통해 ASC가 보관 중인 GA Spec에 접근 가능
	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	FGameplayAbilitySpecHandle AbilityHandle;

	// 장착된 룬 슬롯들
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune")
	TArray<FRuneSlot> RuneSlots;

	// 장착된 초록 룬 (캐싱) - nullptr이면 초록 룬 없음
	UPROPERTY(BlueprintReadOnly, Category = "Rune")
	TObjectPtr<UDA_Rune> EquippedGreenRune;

	// 기본 생성자
	FSkillSlot() : EquippedSkill(nullptr), AbilityHandle(), EquippedGreenRune(nullptr) {
		RuneSlots.SetNum(3); // 룬 슬롯 3칸 확보
	}

	// 초록 룬 캐시 업데이트 (룬 장착/해제 시 호출)
	// 구현은 SkillManagerComponent.cpp에 위치
	void UpdateGreenRuneCache();
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class USkillComponent : public UActorComponent, public ISkillSystemInterface
{
	GENERATED_BODY()

public:
	USkillComponent();

	// 외부(Character/PlayerState)에서 ASC가 준비되었음을 알릴 때 호출
	void InitializeSkillSystem(UAbilitySystemComponent* InASC) override;

	// 로비나 인게임에서 스킬을 교체할 때 호출
	UFUNCTION(BlueprintCallable, Category = "Skill")
	void EquipSkill(FGameplayTag SlotTag, TSubclassOf<UGameplayAbility> AbilityClass);

protected:
	// 실제 어빌리티 부여 로직
	void GiveAbility(FGameplayTag SlotTag, TSubclassOf<UGameplayAbility> AbilityClass);

private:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	/*
	* Gameplay Tag와 Gameplay Ability 클래스를 매핑하는 구조체
	* Tag 신호가 들어오면 해당하는 GA를 부여/실행하기 위함
	* 런타임에 찾아야 하므로 TMap으로 구현
	*/
	UPROPERTY(EditAnywhere, Category = "Skill")
	TMap<FGameplayTag, TSubclassOf<UGameplayAbility>> SkillMap;

	// 현재 장착된 스킬 핸들 관리 (SlotTag -> SpecHandle)
	TMap<FGameplayTag, FGameplayAbilitySpecHandle> ActiveSkillHandles;

	// BP Class Defaults에서 설정하는 초기 스킬셋
	UPROPERTY(EditDefaultsOnly, Category = "Skill")
	TArray<FSkillSlot> DefaultSkillSets;
};