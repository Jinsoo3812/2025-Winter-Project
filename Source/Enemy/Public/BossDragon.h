// BossDragon.h
#pragma once
#include "CoreMinimal.h"
#include "EnemyBase.h" // 부모 클래스 변경
#include "BossDragon.generated.h"

UCLASS()
class ENEMY_API ABossDragon : public AEnemyBase
{
	GENERATED_BODY()

public:
	ABossDragon();

protected:
	// 부모의 BeginPlay(GAS 초기화)를 실행하기 위해 오버라이드
	virtual void BeginPlay() override;
};