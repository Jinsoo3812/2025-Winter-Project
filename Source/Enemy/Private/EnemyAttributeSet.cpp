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

/**
 * [핵심 로직: 스킬 적용 후 처리]
 * 팀원이 만든 스킬(GameplayEffect)이 이 적에게 적용된 "직후"에 호출됩니다.
 * 여기서 최종적인 체력 계산, 사망 처리, 데미지 폰트 띄우기 등을 수행합니다.
 */
void UEnemyAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	FGameplayAttribute ChangedAttribute = Data.EvaluatedData.Attribute;

    // 체력이 변했을 때 로직 수행
    if (ChangedAttribute == GetHealthAttribute())
    {
        // 0. 타겟(맞은 애) 가져오기
        AActor* TargetActor = nullptr;
        if (Data.Target.AbilityActorInfo->AvatarActor.IsValid())
        {
            TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
        }

        // -----------------------------------------------------------------
        // [허수아비 로직] 액터 태그에 "Scarecrow"가 붙어 있는지 확인
        // -----------------------------------------------------------------
        if (TargetActor && TargetActor->ActorHasTag(FName("Scarecrow")))
        {
            // 1. 입은 데미지 계산 (체력이 깎인 양 = Magnitude는 음수이므로 -를 붙임)
            float DamageReceived = -Data.EvaluatedData.Magnitude;

            // 2. 데미지가 있을 때만 로그 출력 (치유나 0 데미지 제외)
            if (DamageReceived > 0.0f)
            {
                // 화면에 붉은색 글씨로 띄우기 (디버깅용)
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
                    FString::Printf(TEXT("[허수아비] 아야! 데미지: %.1f"), DamageReceived));

                // 출력 로그창에 남기기
                UE_LOG(LogTemp, Warning, TEXT("[Scarecrow] Took Damage: %f | Current HP: %f"),
                    DamageReceived, GetHealth());
            }

            // 3. [무한 체력 핵심] 체력을 즉시 최대치로 복구
            SetHealth(GetMaxHealth());

            // 4. 여기서 함수 종료 (아래의 사망 처리 로직으로 넘어가지 않음 -> 절대 안 죽음)
            return;
        }
        // -----------------------------------------------------------------

        // [일반 몬스터 로직]
        // 체력을 0~Max 사이로 자름
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

        // 체력이 0 이하라면 사망 처리
        if (GetHealth() <= 0.0f)
        {
            if (AEnemyBase* Enemy = Cast<AEnemyBase>(TargetActor))
            {
                Enemy->Die(); // 사망 함수 호출
            }
        }
    }
}
