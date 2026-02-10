#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "SkillComponent.generated.h"

class UGameplayAbility;
class UAbilitySystemComponent;
class UDA_Rune;

// 룬 슬롯 구조체
USTRUCT(BlueprintType)
struct FRuneSlot
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UDA_Rune> RuneAsset;

	// 현재 장착된 룬에 의해 부착될 태그
	UPROPERTY(VisibleAnywhere)
	FGameplayTag CachedCombinedTag;
};

// 스킬 슬롯 구조체
USTRUCT(BlueprintType)
struct FSkillSlot
{
	GENERATED_BODY()

public:
	// 슬롯 식별 태그 (ex. Skill.Slot.1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag SlotTag;

	// 장착된 스킬
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayAbility> EquippedSkill;

	// 룬 슬롯 배열
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FRuneSlot> RuneSlots;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SKILL_API USkillComponent : public UActorComponent
{
	GENERATED_BODY()

	// ---------------------------------------------------------------
	// 초기화 및 기본 멤버
	// ---------------------------------------------------------------
public:
	USkillComponent();

	// 복제 설정
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ASC 초기화 및 기본 스킬 장착
	void InitializeSkillSystem(UAbilitySystemComponent* InASC);

	// 스킬 슬롯 개수
	UPROPERTY(EditDefaultsOnly, Category = "SkillSlot")
	int32 SkillSlotCount = 3;

	// 룬 슬롯 개수
	UPROPERTY(EditDefaultsOnly, Category = "SkillSlot")
	int32 RuneSlotCount = 3;

	// 실제 런타임에 운용되는 스킬셋 (Replicated)
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
	TArray<FSkillSlot> SkillSlots;

protected:
	virtual void BeginPlay() override;

	// 에디터에서 설정하는 기본 스킬셋 (테스트용)
	UPROPERTY(EditDefaultsOnly, Category = "Skill")
	TArray<FSkillSlot> DefaultSkills;

	// ASC 캐싱
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	// 활성화된 스킬 핸들 관리 (SlotTag : Handle)
	TMap<FGameplayTag, FGameplayAbilitySpecHandle> ActiveSkillHandles;

	// ---------------------------------------------------------------
	// 스킬 및 룬 장착/해제
	// ---------------------------------------------------------------
public:
	// 스킬 장착
	void EquipSkill(FGameplayTag SlotTag, TSubclassOf<UGameplayAbility> AbilityClass);

	// 룬 장착
	bool EquipRune(int32 SlotIndex, int32 RuneSlotIndex, UDA_Rune* RuneData);

	// 룬 해제
	bool UnequipRune(int32 SlotIndex, int32 RuneSlotIndex);

protected:
	// 룬 태그 업데이트 헬퍼 (Owner ASC에 태그 부착/제거)
	void UpdateRuneTag(int32 SlotIndex, int32 RuneSlotIndex, UDA_Rune* NewRune);

	// 슬롯 태그와 룬 태그를 조합하여 고유 태그 생성 (예: Skill.Slot.1 + Rune.Red -> Skill.Slot.1.Rune.Red)
	FGameplayTag CombineRuneTag(FGameplayTag SlotTag, FGameplayTag RuneTag) const;
};