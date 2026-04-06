// Fill out your copyright notice in the Description page of Project Settings.

#include "Winter2025PlayerAttributeSet.h"
#include "Net/UnrealNetwork.h"        // 네트워크 동기화(DOREPLIFETIME)를 위해 필수
#include "GameplayEffectExtension.h"  // GE 데이터를 다루기 위해 필수
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UWinter2025PlayerAttributeSet::UWinter2025PlayerAttributeSet() {}

void UWinter2025PlayerAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	/*
	* 네트워크 동기화 변수 등록
	* 서버의 Attribute Set이 변하면, 클라이언트의 Attribute Set으로 복제한다.
	* COND_None: 조건 없이 항상 복제
	* REPNOTIFY_Always: 값이 바뀔 때마다 OnRep_XXX 함수를 호출
	*/
	DOREPLIFETIME_CONDITION_NOTIFY(UWinter2025PlayerAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWinter2025PlayerAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWinter2025PlayerAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWinter2025PlayerAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWinter2025PlayerAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWinter2025PlayerAttributeSet, MovementSpeed, COND_None, REPNOTIFY_Always);
}

void UWinter2025PlayerAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	/*
	* 클라이언트 GAS의 예측값과 서버에서 온 실제 값이 다를 수 있으므로
	* 그 둘을 비교해서 동기화 작업을 수행하는 매크로
	*/
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWinter2025PlayerAttributeSet, Health, OldHealth);
}

void UWinter2025PlayerAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWinter2025PlayerAttributeSet, MaxHealth, OldMaxHealth);
}

void UWinter2025PlayerAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWinter2025PlayerAttributeSet, Mana, OldMana);
}

void UWinter2025PlayerAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWinter2025PlayerAttributeSet, MaxMana, OldMaxMana);
}

void UWinter2025PlayerAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWinter2025PlayerAttributeSet, AttackPower, OldAttackPower);
}

void UWinter2025PlayerAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWinter2025PlayerAttributeSet, MovementSpeed, OldMovementSpeed);
}

void UWinter2025PlayerAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// (참고: 체력이 0이 되었을 때 사망 처리 등도 여기서 if (Health)로 처리합니다)


	// [체력 변동 처리]
	// 변경된 속성이 'Health'인 경우에만 아래 로직을 실행합니다.
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// [Clamp 처리: 수치 고정]
		// 체력은 0보다 작아질 수 없고, 최대 체력(MaxHealth)보다 커질 수 없습니다.
		// 예: 체력 100에서 1000 회복을 받아도 100으로 고정.
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

		// [디버깅 로그]
		// 플레이어가 맞았을 때 로그창에 현재 체력을 띄웁니다.
		UE_LOG(LogTemp, Warning, TEXT("[Player] Ouch! Current HP: %f"), GetHealth());

		// [사망 처리]
		if (GetHealth() <= 0.0f)
		{
			UE_LOG(LogTemp, Error, TEXT("[Player] Player Died! (체력이 0이 되었습니다)"));
			// 여기에 사망 애니메이션 재생, 조작 불능 처리 등을 추가하면 됩니다.
		}
	}

	// [마나 변동 처리]
	if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		SetMana(FMath::Clamp(GetMana(), 0.0f, GetMaxMana()));
	}
}




/*
*/
void UWinter2025PlayerAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
}
