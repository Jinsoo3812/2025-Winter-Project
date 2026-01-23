#include "EnemyAttributeSet.h"
#include "Net/UnrealNetwork.h" // DOREPLIFETIME 매크로 사용을 위해 필수
#include "GameplayEffectExtension.h" // PostGameplayEffectExecute 데이터 접근용
#include "EnemyBase.h" // 사망 처리용

UEnemyAttributeSet::UEnemyAttributeSet()
{
	// 필요시 여기서 변수들의 초기값을 설정할 수 있습니다.
	// 보통은 'GE_EnemyBaseStats' 같은 초기화용 GameplayEffect를 사용하므로 비워두기도 합니다.

	//그러나 지금(01/02) 테스트를 위해 잠시 강제 초기화
	// 테스트를 위해 생성자에서 강제로 초기화
	Health.SetBaseValue(100.0f);
	Health.SetCurrentValue(100.0f);
	MaxHealth.SetBaseValue(100.0f);
	MaxHealth.SetCurrentValue(100.0f);
}

/**
 * [네트워크 동기화 규칙 설정]
 * 변수들이 네트워크 상에서 어떻게 복제될지 설정합니다.
 */
void UEnemyAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// DOREPLIFETIME_CONDITION_NOTIFY:
	// 값이 변경될 때마다(Always) 연결된 클라이언트들에게 알립니다(Notify).
	// REPNOTIFY_Always: 값이 서버와 같다 하더라도 명시적으로 알림을 보냅니다 (예측 실수 방지).
	DOREPLIFETIME_CONDITION_NOTIFY(UEnemyAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UEnemyAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

/**
 * [클라이언트 처리: 체력 변경]
 * GAS 시스템에게 "이전 값(OldHealth)이 이거였는데, 새 값으로 갱신해줘"라고 알립니다.
 * 예측(Prediction) 시스템이 정상 작동하도록 돕습니다.
 */
void UEnemyAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEnemyAttributeSet, Health, OldHealth);
}

/**
 * [클라이언트 처리: 최대 체력 변경]
 */
void UEnemyAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEnemyAttributeSet, MaxHealth, OldMaxHealth);
}


void UEnemyAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    // 체력 변경 전 미리 제한 (Clamping) 
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
}


/**
 * [핵심 로직: 스킬 적용 후 처리]
 * 팀원이 만든 스킬(GameplayEffect)이 이 적에게 적용된 "직후"에 호출됩니다.
 * 여기서 최종적인 체력 계산, 사망 처리, 데미지 폰트 띄우기 등을 수행합니다.
 */
void UEnemyAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// Data.Target 자체가 ASC의 레퍼런스입니다.
		UAbilitySystemComponent* TargetASC = &Data.Target;

		// ASC를 통해 AvatarActor(캐릭터)를 직접 가져옵니다. 
		AActor* TargetActor = TargetASC->GetAvatarActor();

        if (IsValid(TargetActor))
        {

            ///////////////////////////////////////////////////////////////////
            // 대미지 및 체력 디버깅 출력
            // Magnitude는 변화량이며, 대미지는 체력을 깎으므로 보통 음수(-)입니다.
            float DamageReceived = -Data.EvaluatedData.Magnitude;

            if (DamageReceived > 0.0f)
            {
                FString DebugMsg = FString::Printf(TEXT("EnemyAttributeSet.cpp : [%s] Damage: %.1f | Now HP : %.1f"),
                    *TargetActor->GetName(), DamageReceived, GetHealth());

                // 화면에 2초간 붉은색으로 출력
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, DebugMsg);
                }
                // 로그창에도 기록
                UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMsg);
            }
            //////////////////////////////////////////////////////////////////

            // [개선 1] 허수아비뿐만 아니라 모든 '무적/무한체력' 상태를 포괄
            // 태그를 State.Invincible 등으로 더 범용적으로 사용하면 보스의 페이즈 전환 무적 등에도 쓸 수 있습니다.
            if (TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Invincible"))) ||
                TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Scarecrow"))))
            {
                SetHealth(GetMaxHealth());
                return;
            }

            // [개선 2] 대미지 처리 전 현재 체력 제한 (Clamping)
            // PreAttributeChange에서도 하지만, 여기서 한 번 더 보정하여 데이터 무결성 확보
            SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

            // 사망 처리
            if (GetHealth() <= 0.0f)
            {
                if (AEnemyBase* Enemy = Cast<AEnemyBase>(TargetActor))
                {
                    // [개선 3] 이미 사망 중인지(bIsDying)와 태그를 이중 확인하여 크래시 방지
                    if (!TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead"))))
                    {
                        Enemy->Die();
                    }
                }
            }
        }
	}
}
