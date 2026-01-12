#include "Player/PlayerAttributeSet.h"
#include "Net/UnrealNetwork.h"        // 네트워크 동기화(DOREPLIFETIME)를 위해 필수
#include "GameplayEffectExtension.h"  // GE 데이터를 다루기 위해 필수
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UPlayerAttributeSet::UPlayerAttributeSet()
{
	// [초기화]
	// 게임 시작 시 플레이어의 체력을 100으로 설정합니다.
	// 나중에는 데이터 테이블(Excel)에서 불러오도록 수정할 수 있습니다.
	Health.SetBaseValue(100.0f);
	Health.SetCurrentValue(100.0f);

	MaxHealth.SetBaseValue(100.0f);
	MaxHealth.SetCurrentValue(100.0f);

	Mana.SetBaseValue(100.0f);
	Mana.SetCurrentValue(100.0f);

	MaxMana.SetBaseValue(100.0f);
	MaxMana.SetCurrentValue(100.0f);

	AttackPower.SetBaseValue(10.0f);
	AttackPower.SetCurrentValue(10.0f);

	MovementSpeed.SetBaseValue(600.0f);
	MovementSpeed.SetCurrentValue(600.0f);
}

void UPlayerAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// [동기화 등록]
	// "Health 변수는 서버에서 바뀌면 무조건(Always) 클라이언트한테 알려줘라"
	DOREPLIFETIME_CONDITION_NOTIFY(UPlayerAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPlayerAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPlayerAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPlayerAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPlayerAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPlayerAttributeSet, MovementSpeed, COND_None, REPNOTIFY_Always);
}

void UPlayerAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	// GAS 시스템에게 "체력 값이 서버로부터 갱신되었습니다"라고 알립니다.
	// 이 코드가 있어야 예측(Prediction) 시스템이 정상 작동합니다.
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPlayerAttributeSet, Health, OldHealth);
}

void UPlayerAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPlayerAttributeSet, MaxHealth, OldMaxHealth);
}

void UPlayerAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPlayerAttributeSet, Mana, OldMana);
}

void UPlayerAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPlayerAttributeSet, MaxMana, OldMaxMana);
}

void UPlayerAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPlayerAttributeSet, AttackPower, OldAttackPower);
}

void UPlayerAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPlayerAttributeSet, MovementSpeed, OldMovementSpeed);

	// 클라이언트도 자기 캐릭터의 MaxWalkSpeed를 맞춰줘야 버벅임(Rubber-banding)이 없음
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (ASC && ASC->GetAvatarActor())
	{
		if (ACharacter* Character = Cast<ACharacter>(ASC->GetAvatarActor()))
		{
			if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
			{
				CMC->MaxWalkSpeed = GetMovementSpeed();
			}
		}
	}
}

void UPlayerAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
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
void UPlayerAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
}
