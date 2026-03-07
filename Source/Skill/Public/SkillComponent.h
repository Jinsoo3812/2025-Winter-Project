#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpecHandle.h"
#include "DA_Rune.h"
#include "BlockSystemInterface.h"
#include "SkillComponent.generated.h"

class UGameplayAbility;
class UAbilitySystemComponent;
class UDA_Rune;

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
	TArray<TObjectPtr<UDA_Rune> > RuneSlots;
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

	// ---------------------------------------------------------------
	// 룬 계산
	// ---------------------------------------------------------------
public:
	/**
	 * 특정 슬롯의 룬 구성을 확인하여 최종 배율을 반환
	 * @param SlotTag 검사할 스킬 슬롯 (ex. Skill.Slot.1)
	 * @param Type 집계할 룬 타입 (ex. Red)
	 * @return 배율 (CurveTable 참조, 실패 시 1.0)
	 */
	float GetTotalRuneMultiplier(FGameplayTag SlotTag, ERuneType Type) const;


	/*
	* 특정 슬롯의 룬 구성을 확인하여 해당 타입의 룬 개수를 반환
	* @param SlotTag 검사할 스킬 슬롯 (ex. Skill.Slot.1)
	* @param Type 집계할 룬 타입 (ex. Red)
	* @return 룬 개수 (실패 시 0)
	*/
	int32 GetTotalRuneCount(FGameplayTag SlotTag, ERuneType Type) const;

protected:
	// 룬 개수에 따른 배율을 정의한 테이블 (X축: 개수, Y축: 배율)
	UPROPERTY(EditDefaultsOnly, Category = "Rune|Config")
	TObjectPtr<UCurveTable> RuneCurveTable;
	
	// ---------------------------------------------------------------
	// 네트워크
	// ---------------------------------------------------------------
public:
	// 외부(GA 등)에서 호출할 일반 함수 (Authority 검사 캡슐화)
	void RequestSpawnBlocks(const TArray<FBlockSpawnRequest>& Requests);

	// 클라이언트의 로컬 스킬 이벤트(클릭 등)를 서버의 ASC(Skill GA)로 전달하는 RPC
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSendSkillEvent(FGameplayTag EventTag);

protected:
	// 클라이언트의 블록 소환 요청을 서버로 전달하는 RPC
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestBlockSpawn(const TArray<FBlockSpawnRequest>& Requests);

	// ---------------------------------------------------------------
	// 캐싱
	// ---------------------------------------------------------------
protected:
	// BlockManagerSubsystem 캐싱
	IBlockSystemInterface* BlockSystem;
};