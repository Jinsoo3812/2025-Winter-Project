// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAI.generated.h"

/**
 * 적 캐릭터의 인공지능을 제어하는 컨트롤러 클래스입니다.
 * 가장 가까운 플레이어를 탐색하고, 비헤이비어 트리를 실행하는 역할을 합니다.
 */
UCLASS()
class ENEMY_API AEnemyAI : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAI();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/**
	 * [UpdateAIState]
	 * 주기적으로 호출되어 AI의 판단을 갱신하는 메인 함수입니다.
	 * 자식 클래스(Dragon)는 이 함수를 오버라이드하여 공격 패턴 등을 추가 판단합니다.
	 */
	virtual void UpdateAIState();

	/**
	 * [FindBestTarget]
	 * 타겟을 선정하는 로직만 분리했습니다.
	 * 기본 동작: 플레이어 태그를 가진 액터 중 가장 가까운 대상 반환.
	 */
	virtual AActor* FindBestTarget();

	/** AI의 행동 로직이 담긴 비헤이비어 트리 에셋 */
	UPROPERTY(EditAnywhere, Category = "AI")
	class UBehaviorTree* BehaviorTreeAsset;

	/** 블랙보드 키 이름 (하드코딩 방지용) */
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	FName BBKey_TargetActor = "TargetActor";

	/** 타겟 탐색 타이머 핸들 */
	FTimerHandle TimerHandle_AIUpdate;
};
