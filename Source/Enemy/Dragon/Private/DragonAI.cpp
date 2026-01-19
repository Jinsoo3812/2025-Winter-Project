#include "DragonAI.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBase.h"
#include "EnemyAttributeSet.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h" //없어도 되는 것 같음


// 키 이름 정의 (이제 "DistanceToTarget" 문자열을 직접 안 써도 됨)
const FName ADragonAI::BBKey_TargetActor(TEXT("TargetActor"));
const FName ADragonAI::BBKey_DistanceToTarget(TEXT("DistanceToTarget"));


ADragonAI::ADragonAI()
{
	// 필요시 초기값 설정
}

void ADragonAI::UpdateAIState()
{
	// [수정 1] 부모 함수(Super) 호출 삭제!
	// 부모는 느린 방식(GetAllActorsWithTag)을 쓰므로 실행할 필요 없습니다.
	// Super::UpdateAIState(); <--- 삭제

	// [수정 2] 폰(Pawn) 캐싱
	// 함수 내에서 여러 번 쓰이므로 변수에 담아둡니다.
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return;

	// 1. 타겟 찾기 (우리가 만든 최적화 버전 사용)
	// 이 함수 안에서 이미 BBKey_TargetActor 값은 갱신됩니다.
	AActor* Target = UpdateTargetToNearestPlayer();

	if (!Blackboard) return;

	// 2. 거리 갱신 로직 (과도한 업데이트 방지)
	if (Target)
	{
		float Distance = MyPawn->GetDistanceTo(Target);

		// [핵심] 기존 값과 10cm 이상 차이 날 때만 블랙보드 업데이트
		// -> Behavior Tree가 거리 변화 때문에 공격을 취소(Abort)하는 것을 방지함
		float OldDistance = Blackboard->GetValueAsFloat(BBKey_DistanceToTarget);

		if (FMath::Abs(Distance - OldDistance) > 10.0f)
		{
			Blackboard->SetValueAsFloat(BBKey_DistanceToTarget, Distance);
		}
	}
	else
	{
		// 타겟이 없으면 아주 먼 거리로 설정
		Blackboard->SetValueAsFloat(BBKey_DistanceToTarget, 99999.0f);
	}

	// 3. 페이즈 판단 로직
	// 위에서 만든 MyPawn 변수를 재사용합니다.
	AEnemyBase* MyEnemy = Cast<AEnemyBase>(MyPawn);
	if (MyEnemy && MyEnemy->GetAttributeSet())
	{
		float Health = MyEnemy->GetAttributeSet()->GetHealth();
		float MaxHealth = MyEnemy->GetAttributeSet()->GetMaxHealth();

		if (MaxHealth > 0.f)
		{
			float HpRatio = Health / MaxHealth;
			int32 CurrentPhase = Blackboard->GetValueAsInt(BBKey_Phase);

			// 체력 50% 이하이고, 아직 2페이즈가 아니라면
			if (HpRatio <= 0.5f && CurrentPhase != 2)
			{
				Blackboard->SetValueAsInt(BBKey_Phase, 2);
				if (GEngine)
					GEngine->AddOnScreenDebugMessage(3, 3.0f, FColor::Red, TEXT("!!! PHASE 2 START !!!"));
			}
		}
	}
}


AActor* ADragonAI::UpdateTargetToNearestPlayer()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn || !Blackboard) return nullptr;

	AActor* NearestPlayer = nullptr;
	float MinDistance = FLT_MAX; // float 최대값으로 초기화

	// [최적화 핵심] 
	// 맵 전체를 뒤지는 대신, '접속한 플레이어 목록'만 순회합니다. (3인 멀티면 딱 3번만 돔)
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			AActor* PlayerActor = PC->GetPawn();

			// 1. 나 자신(혹시 모를 오류 방지) 제외
			if (PlayerActor == MyPawn) continue;

			// 2. 죽은 플레이어 제외 (선택 사항)
			// 만약 플레이어가 죽으면 'Dead' 태그를 붙이거나 Destroy된다면 이 체크가 중요합니다.
			// if (PlayerActor->ActorHasTag("Dead")) continue;

			// 3. 거리 계산
			float Dist = MyPawn->GetDistanceTo(PlayerActor);
			if (Dist < MinDistance)
			{
				MinDistance = Dist;
				NearestPlayer = PlayerActor;
			}
		}
	}

	// 결과 처리
	if (NearestPlayer)
	{
		// 타겟 갱신
		Blackboard->SetValueAsObject(BBKey_TargetActor, NearestPlayer);
		Blackboard->SetValueAsFloat(BBKey_DistanceToTarget, MinDistance);

		// 디버그 로그 (필요 없으면 주석 처리)
		// UE_LOG(LogTemp, Log, TEXT("[DragonAI] New Target: %s (Dist: %.1f)"), *NearestPlayer->GetName(), MinDistance);

		return NearestPlayer;
	}

	UE_LOG(LogTemp, Warning, TEXT("[DragonAI] No Valid Player Found!"));
	return nullptr;
}