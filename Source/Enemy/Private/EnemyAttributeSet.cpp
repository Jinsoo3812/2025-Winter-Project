#include "EnemyAttributeSet.h"
#include "Net/UnrealNetwork.h" // DOREPLIFETIME 매크로 사용을 위해 필수
#include "GameplayEffectExtension.h" // PostGameplayEffectExecute 데이터 접근용

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

	// 변경된 속성(Attribute)이 무엇인지 확인합니다.
	FGameplayAttribute ChangedAttribute = Data.EvaluatedData.Attribute;

	// 1. 체력(Health)이 변경되었을 경우
	if (ChangedAttribute == GetHealthAttribute())
	{
		// [값 보정 (Clamping)]
		// 체력은 0.0보다 작아질 수 없고, MaxHealth보다 커질 수 없습니다.
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

		// [김관희에 의해 수정]
		// 데미지가 제대로 들어갔는지 확인하기 위해 로그를 찍습니다.
		UE_LOG(LogTemp, Warning, TEXT("[Scarecrow] Took Damage! Current HP: %f"), GetHealth());

		// [사망 확인 및 디버깅]
		// 현재 체력이 0 이하이고, 아직 죽은 상태가 아니라면 사망 처리를 할 수 있습니다.
		if (GetHealth() <= 0.0f)
		{
			// [김관희에 의해 수정] 
			// 허수아비로 쓰기 위해 체력을 100으로 다시 채웁니다.
			UE_LOG(LogTemp, Error, TEXT("[Scarecrow] Died! Resetting Health to Max for next test."));
			SetHealth(GetMaxHealth());

			// 예시: 소유자 액터(EnemyBase)를 찾아 죽음 함수 호출
			// AActor* TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
			// if(AEnemyBase* Enemy = Cast<AEnemyBase>(TargetActor)) { Enemy->Die(); }
		}
	}
}