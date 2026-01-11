#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Dragon_RushCheck.generated.h"

/**
 * [UDragon_RushCheck]
 * * 드래곤의 돌진(Rush) 공격 시, 몸통에 붙은 판정 박스(Box Collision)를
 * 특정 구간 동안만 활성화(ON) 시키는 노티파이입니다.
 * * - 동작 원리:
 * 1. NotifyBegin (구간 시작): 박스 콜리전을 켭니다 (QueryOnly).
 * 2. 물리 엔진: 켜진 박스에 플레이어가 닿으면 BossDragon의 OnRushOverlapBegin이 실행됩니다.
 * 3. NotifyEnd (구간 종료): 박스 콜리전을 끕니다 (NoCollision).
 */
UCLASS()
class ENEMY_API UDragon_RushCheck : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// 생성자
	UDragon_RushCheck();

	// [애니메이션 노티파이 구간이 시작될 때 1회 호출]
	// 예: 드래곤이 발을 구르고 앞으로 튀어나가는 순간
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	// [애니메이션 노티파이 구간이 끝날 때 1회 호출]
	// 예: 드래곤이 멈춰 서거나, 돌진 동작이 끝나는 순간
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};