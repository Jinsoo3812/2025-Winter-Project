#include "EnemyBase.h"
#include "AbilitySystemComponent.h"

AEnemyBase::AEnemyBase()
{
	// 1. GAS 컴포넌트 생성
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// 2. 멀티플레이 동기화 설정
	AbilitySystemComponent->SetIsReplicated(true);
}

UAbilitySystemComponent* AEnemyBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}