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
	/**
	 * [OnPossess]
	 * 1. 전체적인 동작 방식:
	 * - AI 컨트롤러가 실제 캐릭터(Pawn)를 조종하기 시작할 때 호출되는 초기화 함수입니다.
	 * 2. 관련 변수:
	 * - InPawn: 이 컨트롤러가 조종하게 될 대상 캐릭터입니다.
	 * - BehaviorTreeAsset: 에디터에서 할당한 AI 로직 에셋입니다.
	 * 3. 상세 동작:
	 * - 부모 클래스의 OnPossess를 실행한 뒤, 설정된 비헤이비어 트리(BT)를 가동합니다.
	 * - 동시에 'UpdateTargetPlayer' 함수가 주기적으로 실행되도록 타이머를 시작합니다.
	 * 4. 사용 가이드:
	 * - 팀원분은 에디터의 AI 컨트롤러 디테일 패널에서 사용할 'BehaviorTreeAsset'을 반드시 할당해 주어야 합니다.
	 */
	virtual void OnPossess(APawn* InPawn) override;

	//타겟을 찾는 로직(주기적으로 호출됨)
	/**
	 * [UpdateTargetPlayer]
	 * 1. 전체적인 동작 방식:
	 * - 월드 내 플레이어들을 실시간으로 감지하여 가장 가까운 대상을 타겟으로 확정합니다.
	 * 2. 관련 변수:
	 * - Players: 월드에서 "Player" 태그를 가진 모든 액터를 담는 배열입니다.
	 * - NearestPlayer: 거리 계산 결과, 현재 가장 가까운 플레이어의 포인터입니다.
	 * - MinDistance: 최소 거리 비교를 위한 기준값입니다.
	 * 3. 상세 동작:
	 * - UGameplayStatics::GetAllActorsWithTag를 통해 타겟 후보군을 빠르게 수집합니다.
	 * - 자신의 위치와 각 플레이어 사이의 거리를 계산(FVector::Dist)하여 최소 거리를 찾습니다.
	 * - 최종 선정된 타겟을 블랙보드의 'TargetActor' 키에 저장하여 비헤이비어 트리가 추격을 시작하게 합니다.
	 * 4. 사용 가이드:
	 * - 중요!! 팀원분은 플레이어 캐릭터 블루프린트의 [Actor -> Tags] 배열에 반드시 "Player"라는 태그를 추가해야 감지가 가능합니다.
	 * - 또한, 블랙보드 에셋에 'TargetActor'라는 이름의 Object 타입 키가 생성되어 있어야 합니다.
	 */
	void UpdateTargetPlayer();

	/** AI의 행동 로직이 담긴 비헤이비어 트리 에셋입니다. (에디터에서 할당 필요) */
	UPROPERTY(EditAnywhere, Category = "AI")
	UBehaviorTree* BehaviorTreeAsset;

	/** 타겟 탐색 주기를 관리하는 타이머 핸들. */
	FTimerHandle TargetUpdateTimerHandle;
};
