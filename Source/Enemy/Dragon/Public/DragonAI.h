#pragma once

#include "CoreMinimal.h"
#include "EnemyAI.h" // 부모 클래스 포함
#include "DragonAI.generated.h"

// 드래곤의 공격 패턴을 정의하는 열거형
UENUM(BlueprintType)
enum class EDragonAttackType : uint8
{
	None = 0 UMETA(DisplayName = "None"),			// 공격 안함 (추격 중)
	Melee_Claw UMETA(DisplayName = "Melee Claw"),			// 2연타 앞발 (가까움)
	Rush_Charge UMETA(DisplayName = "Rush Charge")		// 이속 디버프 함성 이후 돌진
};

/**
 * [Child Class] ADragonAI
 * -----------------------------------------------------------
 * 역할: 보스 드래곤의 두뇌(Brain) 역할을 하는 AI 컨트롤러입니다.
 * 기능:
 * 1. 가장 가까운 플레이어를 찾아 타겟팅 (UpdateTargetToNearestPlayer)
 * 2. 거리 및 체력(HP) 상태를 체크하여 페이즈(Phase) 전환 관리
 * 3. 블랙보드(Blackboard)에 현재 상태를 기록하여 비헤이비어 트리(BT)가 행동을 결정하게 함
 */
UCLASS()
class ENEMY_API ADragonAI : public AEnemyAI
{
	GENERATED_BODY()

public:
	ADragonAI();

	// 가장 가까운 플레이어를 찾아 타겟으로 지정하고 리턴하는 함수
	// BlueprintCallable: 비헤이비어 트리 태스크(BTTask)나 블루프린트에서 직접 호출 가능
	UFUNCTION(BlueprintCallable, Category = "Dragon AI")
	AActor* UpdateTargetToNearestPlayer();

protected:
	/** * [핵심 루프] 매 틱(Tick) 혹은 타이머마다 호출되어 AI의 판단을 수행합니다.
	 * 부모(AEnemyAI)의 기본 로직 + 드래곤 전용(페이즈, 특수 패턴) 로직을 실행합니다.
	 */
	virtual void UpdateAIState() override;

	/** AI가 폰(Pawn)에 빙의(Possess)될 때 호출 (초기화 담당) */
	virtual void OnPossess(APawn* InPawn) override;

protected:
	// --- 에디터 설정값 (파라미터) ---

	// 근접 공격(앞발) 발동 거리 (단위: cm)
	UPROPERTY(EditDefaultsOnly, Category = "Dragon AI")
	float MeleeRange = 350.0f;

	// 돌진 패턴 발동 거리
	UPROPERTY(EditDefaultsOnly, Category = "Dragon AI")
	float RushRange = 1200.0f;

	// --- 블랙보드 키 이름 (Blackboard Key Names) ---
	// 에디터에서 키 이름을 바꿀 경우를 대비해 변수로 관리합니다.

	UPROPERTY(EditDefaultsOnly, Category = "Dragon AI|Blackboard Keys")
	FName BBKey_AttackType = "AttackType"; // 공격 종류 (Enum)

	UPROPERTY(EditDefaultsOnly, Category = "Dragon AI|Blackboard Keys")
	FName BBKey_Phase = "Phase"; // 현재 페이즈 (0:일반, 1:66%패턴, 2:33%패턴)

	UPROPERTY(EditDefaultsOnly, Category = "Dragon AI|Blackboard Keys")
	FName BBKey_CastingWipe = "IsCastingWipe"; // 전멸기(Wipe) 시전 중인가? (Bool)

public:
	// C++ 코드 내에서 편하게 쓰기 위한 정적 상수 키 이름들
	// (헤더에서는 선언만 하고, cpp에서 정의함)
	static const FName Key_TargetActor;       // 타겟 액터 (Object)
	static const FName Key_DistanceToTarget;  // 타겟과의 거리 (Float)
};