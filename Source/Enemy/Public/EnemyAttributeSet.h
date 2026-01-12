#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EnemyAttributeSet.generated.h"

// GAS 매크로: 속성 접근자(Getter/Setter) 자동 생성
/**
 * [매크로 설명]
 * 언리얼 GAS에서 속성(Attribute)에 접근하기 위한 보일러플레이트 코드를 자동으로 생성해줍니다.
 * - GetHealth(): 현재 체력 값을 가져옵니다.
 * - SetHealth(): 현재 체력 값을 강제로 설정합니다.
 * - InitHealth(): 초기 체력 값을 설정합니다.
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
		GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

 /**
  * UEnemyAttributeSet
  * * [클래스 개요]
  * 적(Enemy) 캐릭터가 가지는 모든 '수치 데이터(Stats)'를 정의하고 관리하는 클래스입니다.
  * 팀원(네트워크/스킬 담당)은 GameplayEffect를 통해 이 클래스의 변수들을 변경하여 데미지를 줍니다.
  * * [동작 방식]
  * 1. Health, MaxHealth 등의 변수를 가집니다.
  * 2. 네트워크 리플리케이션(Replication)을 지원하여 서버의 체력 변경 사항을 모든 클라이언트에게 동기화합니다.
  * 3. 체력이 변경될 때(PostGameplayEffectExecute), 0 이하로 떨어지는지 검사하여 사망 여부를 판단할 수 있습니다.
  */
UCLASS()
class ENEMY_API UEnemyAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	// 생성자: 속성들의 초기화를 담당할 수 있습니다.
	UEnemyAttributeSet();

	// [네트워크 동기화 필수 함수]
	// 어떤 변수들이 네트워크를 통해 복제될지(Replicated) 규칙을 정의합니다.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// [값 변경 처리 함수]
	// GameplayEffect(스킬 등)가 실행된 직후 호출됩니다.
	// 예: 데미지를 입어 Health가 감소했을 때, 음수가 되지 않도록 0으로 보정(Clamp)하는 로직이 들어갑니다.
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	// -------------------------------------------------------------------------
	// 속성 정의 (Attributes)
	// -------------------------------------------------------------------------

	/**
	 * [Health: 현재 체력]
	 * - ReplicatedUsing = OnRep_Health: 서버에서 값이 바뀌면 클라이언트의 OnRep_Health 함수가 호출됩니다.
	 * - FGameplayAttributeData: GAS에서 값을 저장하는 전용 구조체입니다 (BaseValue, CurrentValue 포함).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, Health);

	/**
	 * [MaxHealth: 최대 체력]
	 * - 체력 회복 시 현재 체력이 이 값을 넘지 않도록 제한하는 용도로 쓰입니다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UEnemyAttributeSet, MaxHealth);

protected:
	// -------------------------------------------------------------------------
	// OnRep 함수들 (클라이언트 동기화 처리)
	// -------------------------------------------------------------------------

	/**
	 * 서버에서 Health 값이 변경되었다는 신호를 받았을 때 클라이언트에서 실행됩니다.
	 * 주로 체력바 UI를 갱신하거나, 피격 효과를 연출할 때 사용됩니다.
	 */
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	/**
	 * 서버에서 MaxHealth 값이 변경되었을 때 클라이언트에서 실행됩니다.
	 */
	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
};
