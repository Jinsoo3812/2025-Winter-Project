#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Engine/DataTable.h"
#include "Winter2025PlayerAttributeSet.generated.h"

// [매크로] Getter, Setter, Init 함수 자동 생성기
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
		GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 플레이어 초기 스탯 데이터 테이블 구조체
 * 레벨별로 플레이어의 초기 능력치를 정의
 */
USTRUCT(BlueprintType)
struct FPlayerInitialStatsRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 플레이어 레벨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 Level = 1;

	// 초기 체력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float InitialHealth = 100.0f;

	// 초기 최대 체력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float InitialMaxHealth = 100.0f;

	// 초기 마나
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float InitialMana = 100.0f;

	// 초기 최대 마나
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float InitialMaxMana = 100.0f;

	// 초기 공격력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float InitialAttackPower = 10.0f;

	// 초기 이동속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float InitialMovementSpeed = 600.0f;
};

/**
 * UWinter2025PlayerAttributeSet
 * * [역할]
 * 플레이어의 체력, 최대 체력 등의 수치 데이터를 보관.
 * 플레이어가 데미지를 입으면 이 클래스의 함수들이 호출되어 수치를 깎습니다.
 */
UCLASS()
class UWinter2025PlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	// 생성자: 변수 초기화를 담당합니다.
	UWinter2025PlayerAttributeSet();

	// [네트워크 필수] 변수 동기화 규칙을 설정하는 함수입니다.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// [GAS 핵심] GE(Gameplay Effect)가 적용된 '직후'에 호출되는 함수입니다.
	// 예: 데미지를 맞고 나서 체력이 -10이 된 순간, 체력이 0 밑으로 안 내려가게 막는 처리를 여기서 합니다.
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	// -------------------------------------------------------------------------
	// 속성(Attribute) 정의
	// -------------------------------------------------------------------------

	/**
	 * [Health: 현재 체력]
	 * - ReplicatedUsing = OnRep_Health: 서버에서 체력이 바뀌면 클라이언트(플레이어 화면)의 OnRep_Health 함수를 실행해라! 라는 뜻입니다.
	 * - 즉, 서버가 "너 맞았어" 하면 클라이언트가 "아윽!" 하고 반응하게 만듭니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UWinter2025PlayerAttributeSet, Health);

	/**
	 * [MaxHealth: 최대 체력]
	 * - 체력 회복 시 이 수치를 넘지 않도록 제한하는 기준값이 됩니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UWinter2025PlayerAttributeSet, MaxHealth);

	/**
	 * [Mana: 현재 마나]
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Mana)
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UWinter2025PlayerAttributeSet, Mana);

	/**
	 * [MaxMana: 최대 마나]
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxMana)
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UWinter2025PlayerAttributeSet, MaxMana);

	/**
	 * [AttackPower: 공격력]
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_AttackPower)
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UWinter2025PlayerAttributeSet, AttackPower);

	/**
	 * [MovementSpeed: 이동속도]
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MovementSpeed)
	FGameplayAttributeData MovementSpeed;
	ATTRIBUTE_ACCESSORS(UWinter2025PlayerAttributeSet, MovementSpeed);


	// Attribute 수치가 변하면(버프/디버프 포함) 무조건 호출되는 함수
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

protected:
	// [OnRep 함수들]
	// 서버로부터 바뀐 값을 받아왔을 때, 클라이언트 쪽에서 갱신 처리를 하는 함수들입니다.
	// UI 업데이트(체력바 줄어듦) 등을 여기서 연결할 수도 있습니다.

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	virtual void OnRep_Mana(const FGameplayAttributeData& OldMana);

	UFUNCTION()
	virtual void OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana);

	UFUNCTION()
	virtual void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);

	UFUNCTION()
	virtual void OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed);
};
