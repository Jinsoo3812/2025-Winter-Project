#include "DragonAI.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBase.h"
#include "EnemyAttributeSet.h"

ADragonAI::ADragonAI()
{
	// 필요시 초기값 설정
}

void ADragonAI::UpdateAIState()
{
	Super::UpdateAIState(); // 1. 타겟 찾기 실행

	if (!Blackboard) return;

	// 블랙보드에서 타겟 가져오기
	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(BBKey_TargetActor));
	APawn* MyPawn = GetPawn();

	// --- [디버깅] 타겟 상태 확인 ---
	if (!Target)
	{
		// 타겟이 없으면 노란색 메시지
		if (GEngine) GEngine->AddOnScreenDebugMessage(1, 1.0f, FColor::Yellow, TEXT("Dragon AI: No Target Found!"));

		// 공격/이동 모두 멈춤
		Blackboard->SetValueAsEnum(BBKey_AttackType, (uint8)EDragonAttackType::None);
		return;
	}

	if (MyPawn)
	{
		// 2. 거리 계산
		float Distance = MyPawn->GetDistanceTo(Target);

		// 3. 공격 타입 결정
		uint8 SelectedAttack = (uint8)EDragonAttackType::None; // 0: 추격만 함

		FString StateString = TEXT("Chasing"); // 디버그용 문자열

		if (Distance <= MeleeRange) // 350.0f
		{
			SelectedAttack = (uint8)EDragonAttackType::Melee_Claw;
			StateString = TEXT("ATTACK: Melee Claw");
		}
		else if (Distance <= BreathRange) // 1200.0f
		{
			SelectedAttack = (uint8)EDragonAttackType::Range_Breath;
			StateString = TEXT("ATTACK: Breath");
		}

		// 블랙보드 업데이트
		Blackboard->SetValueAsEnum(BBKey_AttackType, SelectedAttack);

		// --- [디버깅] 현재 상태 화면 출력 (초록색) ---
		// 예: "Dist: 500.0 / State: ATTACK: Breath"
		if (GEngine)
		{
			FString Msg = FString::Printf(TEXT("Dist: %.1f / State: %s"), Distance, *StateString);
			GEngine->AddOnScreenDebugMessage(2, 1.0f, FColor::Green, Msg);
		}
	}

	// 4. 페이즈 판단 로직 (생략 없이 그대로 유지)
	AEnemyBase* MyEnemy = Cast<AEnemyBase>(MyPawn);
	if (MyEnemy && MyEnemy->GetAttributeSet())
	{
		float Health = MyEnemy->GetAttributeSet()->GetHealth();
		float MaxHealth = MyEnemy->GetAttributeSet()->GetMaxHealth();

		if (MaxHealth > 0.f)
		{
			float HpRatio = Health / MaxHealth;
			int32 CurrentPhase = Blackboard->GetValueAsInt(BBKey_Phase);

			if (HpRatio <= 0.5f && CurrentPhase != 2)
			{
				Blackboard->SetValueAsInt(BBKey_Phase, 2);
				if (GEngine) GEngine->AddOnScreenDebugMessage(3, 3.0f, FColor::Red, TEXT("!!! PHASE 2 START !!!"));
			}
		}
	}
}