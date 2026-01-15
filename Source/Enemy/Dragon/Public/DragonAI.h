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
 * [Child Class]
 * 드래곤 보스 전용 AI입니다.
 * 타겟과의 거리에 따라 공격 패턴을 결정하여 블랙보드에 전달합니다.
 */
UCLASS()
class ENEMY_API ADragonAI : public AEnemyAI
{
	GENERATED_BODY()

public:
	ADragonAI();

	virtual void Tick(float DeltaTime) override;

protected:
	/** 부모의 로직(타겟 찾기) + 드래곤 전용 로직(공격 판단) 실행 */
	virtual void UpdateAIState() override;

protected:
	// --- 설정값 (에디터에서 수정 가능) ---
	UPROPERTY(EditDefaultsOnly, Category = "Dragon AI")
	float MeleeRange = 350.0f;  // 앞발 공격 사거리

	UPROPERTY(EditDefaultsOnly, Category = "Dragon AI")
	float RushRange = 1200.0f; // 브레스 공격 사거리

	// --- 블랙보드 키 이름 ---
	UPROPERTY(EditDefaultsOnly, Category = "Dragon AI")
	FName BBKey_AttackType = "AttackType"; // 공격 타입 (Enum)

	UPROPERTY(EditDefaultsOnly, Category = "Dragon AI")
	FName BBKey_Phase = "Phase"; // 현재 페이즈 (1, 2, 3...)

	static const FName BBKey_DistanceToTarget;
};
