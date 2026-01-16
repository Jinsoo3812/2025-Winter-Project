// Fill out your copyright notice in the Description page of Project Settings.


#include "Block/BlockAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "Block/DestructibleBlock.h" // ADestructibleBlock의 헤더 필요

UBlockAttributeSet::UBlockAttributeSet()
{
	// 생성자에서 초기값 설정
	// 참고: 실제 게임에서는 GameplayEffect를 통해 초기화하는 것이 정석이지만, 
	// 단순한 액터의 경우 생성자 초기화도 유효함.
	InitHealth(1.f);
	InitMaxHealth(1.f);
}

void UBlockAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	/*
	* 값이 변경되면 무조건 알림
	* COND_None: 조건 없이항상 복제
	* REPNOTIFY_Always: 값이 변경될 때마다 OnRep 함수 호출
	*/
	DOREPLIFETIME_CONDITION_NOTIFY(UBlockAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBlockAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UBlockAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 체력은 0 ~ MaxHealth 사이로 제한
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
}

void UBlockAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 변경된 속성이 체력인 경우
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// 체력이 0 이하인지 확인
		if (GetHealth() <= 0.0f)
		{
			// GE 컨텍스트에서 가해자(Instigator) 추출
			AActor* Instigator = Data.EffectSpec.GetContext().GetInstigator();

			if (OnDestroyed.IsBound())
			{
				OnDestroyed.Broadcast(Instigator);
			}
		}
	}
}

void UBlockAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	/*
	* 클라이언트 GAS의 예측값과 서버에서 온 실제 값이 다를 수 있으므로
	* 그 둘을 비교해서 동기화 작업을 수행하는 매크로
	*/
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBlockAttributeSet, Health, OldHealth);
}

void UBlockAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBlockAttributeSet, MaxHealth, OldMaxHealth);
}

