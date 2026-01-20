// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BlockAttributeSet.generated.h"

// 게터, 세터, 초기화 함수를 자동으로 생성하는 매크로
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

// 파괴 시 호출할델리게이트 선언
// 파라미터로 누가 죽였는지 전달
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDestroyed, AActor*);

/**
 * Block Attribute Set
 */
UCLASS()
class WORLD_API UBlockAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UBlockAttributeSet();

	// 네트워크 복제를 위한 함수 (서버에서 값이 변경되었을 때, 클라이언트에 동기화)
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 속성 변경 전 호출
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// GE를 적용 받은 후 호출 (PreAttributeChange 이후)
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// 파괴되었을 때 방송할 델리게이트
	mutable FOnDestroyed OnDestroyed;

public:
	// 체력
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UBlockAttributeSet, Health);

	// 최대 체력
	UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UBlockAttributeSet, MaxHealth);

protected:
	// OnRep 함수들 (서버에서 변경된 값이 클라이언트에 도착했을 때 호출되는 콜백 함수)
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
};
