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

		// [중요] 최적화 및 변수 충돌 방지
		// AEnemyBase로의 캐스팅을 여기서 딱 한 번만 수행하여 재사용합니다.
		AEnemyBase* Enemy = Cast<AEnemyBase>(TargetActor);

		if (IsValid(TargetActor))
		{
			// 1. 대미지 수치 확인 및 UI 출력 요청
			// Magnitude는 변화량이며, 대미지는 체력을 깎으므로 보통 음수(-)입니다.
			// 이를 양수로 변환하여 UI에 전달합니다.
			float DamageReceived = -Data.EvaluatedData.Magnitude;

			if (DamageReceived > 0.0f)
			{

				// ----------------------------------------------------------------
				// [디버깅] 대미지가 들어왔을 때 로그와 화면에 출력합니다.
				FString DebugMsg = FString::Printf(TEXT("EnemyAttributeSet : [%s] Took Damage: %.1f | Current HP : %.1f"),
					*TargetActor->GetName(), DamageReceived, GetHealth());

				// (1) 화면에 2초간 붉은색 글씨로 출력
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, DebugMsg);
				}

				// (2) 출력 로그(Output Log) 창에도 기록
				UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMsg);
				// ----------------------------------------------------------------
				

				// EnemyBase를 상속받은 모든 몬스터(보스, 쫄몹, 허수아비 등)에게 작동합니다.
				if (Enemy)
				{
					// BlueprintImplementableEvent 호출 -> BP에서 위젯 스폰 로직 실행
					Enemy->ShowDamageNumber(DamageReceived);
				}
			}

			// [특수 상태 처리] 무적(Invincible) 또는 허수아비(Scarecrow) 상태 확인
			// 이 상태에서는 체력이 줄어들지 않고 즉시 회복됩니다.
			// (대미지 폰트는 위에서 이미 출력되었으므로 타격감은 유지됩니다.)
			if (TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Invincible"))) ||
				TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Scarecrow"))))
			{
				SetHealth(GetMaxHealth());
				return;
			}

			// 2. 체력 값 보정 (Clamping)
			// 체력이 0 미만으로 떨어지거나 최대 체력을 초과하지 않도록 보정합니다.
			SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

			// 3. 사망 처리
			// 체력이 0 이하가 되었을 때 사망 로직을 수행합니다.
			if (GetHealth() <= 0.0f)
			{
				if (Enemy)
				{
					// 이미 사망 상태(State.Dead)가 아닐 때만 사망 함수 호출 (중복 사망 방지)
					if (!TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead"))))
					{
						Enemy->Die();
					}
				}
			}
		}
	}
}
