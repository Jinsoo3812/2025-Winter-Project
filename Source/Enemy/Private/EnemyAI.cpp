#include "EnemyAI.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

AEnemyAI::AEnemyAI()
{
    // 블랙보드와 비헤이비어 트리 컴포넌트는 기본 제공됨
}

void AEnemyAI::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (BehaviorTreeAsset)
    {
        // 비헤이비어 트리 실행
        RunBehaviorTree(BehaviorTreeAsset);
        // 0.5초마다 가장 가까운 플레이어를 체크하도록 타이머 설정 (최적화 위해 주기 조절 가능)
        GetWorld()->GetTimerManager().SetTimer(TargetUpdateTimerHandle, this, &AEnemyAI::UpdateTargetPlayer, 0.5f, true);
    }
}

void AEnemyAI::UpdateTargetPlayer()
{
    APawn* OwningPawn = GetPawn();
    if (!OwningPawn) return;

    // 1. 월드 내 모든 플레이어(태그 기준) 수집
    TArray<AActor*> Players;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Player"), Players); // 플레이어 태그 활용

    AActor* NearestPlayer = nullptr;
    float MinDistance = MAX_FLT;

    // 2. 거리 계산 및 최소 거리 타겟 선정
    for (AActor* Player : Players)
    {
        float Distance = FVector::Dist(OwningPawn->GetActorLocation(), Player->GetActorLocation());
        if (Distance < MinDistance)
        {
            MinDistance = Distance;
            NearestPlayer = Player;
        }
    }

    // 3. 블랙보드 데이터 업데이트
    if (NearestPlayer)
    {
        // 팀원분들이 블랙보드에 만든 'TargetActor' 변수와 이름이 같아야 함
        GetBlackboardComponent()->SetValueAsObject(FName("TargetActor"), NearestPlayer);
    }
}