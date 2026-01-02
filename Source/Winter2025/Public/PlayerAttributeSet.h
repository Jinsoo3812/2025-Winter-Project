#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "PlayerAttributeSet.generated.h"

// [매크로] Getter, Setter, Init 함수 자동 생성기
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
		GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * UPlayerAttributeSet
 * * [역할]
 * 플레이어의 체력, 최대 체력 등의 수치 데이터를 보관.
 * 플레이어가 데미지를 입으면 이 클래스의 함수들이 호출되어 수치를 깎습니다.
 */
UCLASS()
class WINTER2025_API UPlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	// 생성자: 변수 초기화를 담당합니다.
	UPlayerAttributeSet();

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
	ATTRIBUTE_ACCESSORS(UPlayerAttributeSet, Health);

	/**
	 * [MaxHealth: 최대 체력]
	 * - 체력 회복 시 이 수치를 넘지 않도록 제한하는 기준값이 됩니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UPlayerAttributeSet, MaxHealth);

protected:
	// [OnRep 함수들]
	// 서버로부터 바뀐 값을 받아왔을 때, 클라이언트 쪽에서 갱신 처리를 하는 함수들입니다.
	// UI 업데이트(체력바 줄어듦) 등을 여기서 연결할 수도 있습니다.

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
};