#include "DragonAI.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBase.h"
#include "EnemyAttributeSet.h"


// [추가 1] 키 이름 정의 (이제 "DistanceToTarget" 문자열을 직접 안 써도 됨)
const FName ADragonAI::BBKey_DistanceToTarget(TEXT("DistanceToTarget"));


ADragonAI::ADragonAI()
{
	// 필요시 초기값 설정
}

void ADragonAI::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 매 프레임 AI 상태(거리 등)를 갱신
	UpdateAIState();
}

void ADragonAI::UpdateAIState()
{
	Super::UpdateAIState(); // 1. 타겟 찾기 실행

	if (!Blackboard) return;

	// 블랙보드에서 타겟 가져오기
	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(BBKey_TargetActor));
	APawn* MyPawn = GetPawn();

	// --- [디버깅] 타겟 상태 확인 ---
	if (MyPawn && Target)
	{
		// 2. 거리 계산
		float Distance = MyPawn->GetDistanceTo(Target);

		// 공격 타입을 여기서 정하지 말고, "거리"만 블랙보드에 저장합니다.
		// 블랙보드에 'DistanceToTarget'이라는 키가 있다고 가정하고 값을 쏴줍니다.
		// (블랙보드 키 이름은 프로젝트 설정에 맞춰주세요. 보통 FName 변수로 관리하시겠지만, 일단 문자열로 적습니다.)
		Blackboard->SetValueAsFloat(BBKey_DistanceToTarget, Distance);

		// --- [디버깅] 거리 확인용 ---
		if (GEngine)
		{
			FString Msg = FString::Printf(TEXT("Dragon Eye: Distance %.1f"), Distance);
			GEngine->AddOnScreenDebugMessage(2, 1.0f, FColor::Green, Msg);
		}
	}
	else
	{
		// 타겟 없으면 거리 0 (또는 아주 큰 값)
		Blackboard->SetValueAsFloat(BBKey_DistanceToTarget, 99999.0f);
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
