#include "EnemyAI.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTree.h" // 헤더 추가 필요
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

AEnemyAI::AEnemyAI()
{
	// 기본값 설정
}

void AEnemyAI::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (BehaviorTreeAsset)
	{

		//비헤이비어 트리 실행
		RunBehaviorTree(BehaviorTreeAsset);

		//0.5초마다 AI 상태 갱신
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_AIUpdate, this, &AEnemyAI::UpdateAIState, 0.5f, true);
	}
}

void AEnemyAI::OnUnPossess()
{
	Super::OnUnPossess();
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_AIUpdate);
}

void AEnemyAI::UpdateAIState()
{
	// 부모 클래스는 단순히 타겟만 찾아서 블랙보드에 넣습니다.
	AActor* NewTarget = FindBestTarget();

	if (Blackboard)
	{
		Blackboard->SetValueAsObject(BBKey_TargetActor, NewTarget);
	}
}

AActor* AEnemyAI::FindBestTarget()
{
	APawn* OwningPawn = GetPawn();
	if (!OwningPawn) return nullptr;

	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), Players);

	AActor* NearestPlayer = nullptr;
	float MinDistanceSq = FLT_MAX; // 제곱 거리 사용 (연산 최적화)
	FVector MyLoc = OwningPawn->GetActorLocation();

	for (AActor* Player : Players)
	{
		if (!Player) continue;

		// DistSquared가 Sqrt 연산이 없어 더 빠릅니다.
		float DistSq = FVector::DistSquared(MyLoc, Player->GetActorLocation());
		if (DistSq < MinDistanceSq)
		{
			MinDistanceSq = DistSq;
			NearestPlayer = Player;
		}
	}

	return NearestPlayer;
}
