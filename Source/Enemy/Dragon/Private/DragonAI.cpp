#include "DragonAI.h"

// [필수 헤더]
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTree.h" // RunBehaviorTree용
#include "EnemyBase.h"                 // 드래곤 본체 (AEnemyBase 상속)
#include "Enemy/Public/EnemyAttributeSet.h" // 체력 정보 조회용
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

// ---------------------------------------------------------------------------
// [Static Const Definitions] 블랙보드 키 이름 상수 정의
// ---------------------------------------------------------------------------
const FName ADragonAI::Key_TargetActor(TEXT("TargetActor"));
const FName ADragonAI::Key_DistanceToTarget(TEXT("DistanceToTarget"));


// ---------------------------------------------------------------------------
// [Constructor] 생성자
// ---------------------------------------------------------------------------
ADragonAI::ADragonAI()
{
	// AI 컨트롤러는 기본적으로 틱(Tick)이 켜져 있을 수 있으나,
	// 성능 최적화를 위해 필요할 때만 켜거나 Timer를 쓰는 것이 좋습니다.
	// AEnemyAI(부모)에서 타이머를 쓰고 있다면 그대로 둡니다.
}


// ---------------------------------------------------------------------------
// [OnPossess] AI가 폰을 제어하기 시작할 때
// ---------------------------------------------------------------------------
void ADragonAI::OnPossess(APawn* InPawn)
{
	// 1. 부모 클래스(AEnemyAI)의 초기화 로직 실행 (타이머 구동 등)
	Super::OnPossess(InPawn);

	// 2. 드래곤 전용 비헤이비어 트리(BT) 실행
	// 에디터에서 AI Controller의 'BehaviorTreeAsset' 속성에 'BT_BossDragon'을 할당해야 동작합니다.
	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);
		UE_LOG(LogTemp, Log, TEXT("[DragonAI] Possessed Pawn: %s. Behavior Tree Started!"), *InPawn->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[DragonAI] BehaviorTreeAsset is NULL! Please assign BT_BossDragon in Editor."));
	}
}


// ---------------------------------------------------------------------------
// [UpdateAIState] 주기적인 판단 로직 (타이머에 의해 호출됨)
// ---------------------------------------------------------------------------
void ADragonAI::UpdateAIState()
{
	// 폰이나 블랙보드가 없으면 로직 수행 불가
	APawn* MyPawn = GetPawn();
	if (!MyPawn || !Blackboard) return;

	// -------------------------------------------------------
	// 1. 타겟 탐색 및 거리 갱신
	// -------------------------------------------------------
	AActor* Target = UpdateTargetToNearestPlayer();

	if (Target)
	{
		// 타겟이 있다면 거리 계산 후 블랙보드 업데이트
		float Distance = MyPawn->GetDistanceTo(Target);

		// 불필요한 연산 방지: 거리가 유의미하게(10cm 이상) 변했을 때만 값 갱신
		float OldDistance = Blackboard->GetValueAsFloat(Key_DistanceToTarget);
		if (FMath::Abs(Distance - OldDistance) > 10.0f)
		{
			Blackboard->SetValueAsFloat(Key_DistanceToTarget, Distance);
		}
	}
	else
	{
		// 타겟이 없으면 거리를 무한대(매우 큰 값)로 설정하여 추격 중지 유도
		Blackboard->SetValueAsFloat(Key_DistanceToTarget, 99999.0f);
	}

	// -------------------------------------------------------
	// 2. 체력 기반 페이즈(Phase) 전환 로직 (즉사기 발동 체크)
	// -------------------------------------------------------
	AEnemyBase* MyEnemy = Cast<AEnemyBase>(MyPawn);

	// GAS(AttributeSet)를 통해 정확한 체력 정보를 가져옴
	if (MyEnemy && MyEnemy->GetAttributeSet())
	{
		float Health = MyEnemy->GetAttributeSet()->GetHealth();
		float MaxHealth = MyEnemy->GetAttributeSet()->GetMaxHealth();

		if (MaxHealth > 0.f)
		{
			float HpRatio = Health / MaxHealth;

			// 블랙보드에서 현재 페이즈 가져오기 (초기값: 0)
			int32 CurrentPhase = Blackboard->GetValueAsInt(BBKey_Phase);

			// [조건 1] 체력 66% 이하 + 아직 1페이즈 안 함 (Phase 0 -> 1)
			if (HpRatio <= 0.66f && CurrentPhase == 0)
			{
				Blackboard->SetValueAsInt(BBKey_Phase, 1);       // 페이즈 1 진입
				Blackboard->SetValueAsBool(BBKey_CastingWipe, true); // 즉사기(Wipe) 시전 플래그 ON

				// 디버그 메시지
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("!!! PHASE 1 (66%) - WIPE PATTERN !!!"));
				UE_LOG(LogTemp, Warning, TEXT("[DragonAI] HP %.1f%% -> Phase 1 Started!"), HpRatio * 100.0f);
			}
			// [조건 2] 체력 33% 이하 + 1페이즈는 끝남 (Phase 1 -> 2)
			else if (HpRatio <= 0.33f && CurrentPhase == 1)
			{
				Blackboard->SetValueAsInt(BBKey_Phase, 2);       // 페이즈 2 진입
				Blackboard->SetValueAsBool(BBKey_CastingWipe, true); // 즉사기(Wipe) 시전 플래그 ON

				// 디버그 메시지
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("!!! PHASE 2 (33%) - WIPE PATTERN !!!"));
				UE_LOG(LogTemp, Warning, TEXT("[DragonAI] HP %.1f%% -> Phase 2 Started!"), HpRatio * 100.0f);
			}
		}
	}
}


// ---------------------------------------------------------------------------
// [UpdateTargetToNearestPlayer] 가장 가까운 플레이어 찾기
// ---------------------------------------------------------------------------
AActor* ADragonAI::UpdateTargetToNearestPlayer()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn || !Blackboard) return nullptr;

	AActor* NearestPlayer = nullptr;
	float MinDistance = FLT_MAX; // 비교를 위해 최대값으로 초기화
	FVector MyLoc = MyPawn->GetActorLocation();

	// [최적화 핵심] 
	// GetAllActorsOfClass 대신 PlayerControllerIterator를 사용합니다.
	// 월드에 존재하는 모든 액터가 아니라, '접속 중인 플레이어'만 순회하므로 매우 빠릅니다.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (APawn* PlayerPawn = PC->GetPawn())
			{
				// 거리 계산 공식: d^2 = (x_2-x_1)^2 + (y_2-y_1)^2 + (z_2-z_1)^2
				float DistSq = FVector::DistSquared(MyLoc, PlayerPawn->GetActorLocation());
				if (DistSq < MinDistance)
				{
					MinDistance = DistSq;
					NearestPlayer = PlayerPawn;
				}
			}
		}
	}

	if (NearestPlayer && Blackboard)
	{
		Blackboard->SetValueAsObject(Key_TargetActor, NearestPlayer);
	}
	return NearestPlayer;
}