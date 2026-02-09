#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "BTTask_WaitGameplayTagRemoved.generated.h"

/**
 * 지정된 GameplayTag가 사라질 때까지 비헤이비어 트리를 대기(In Progress) 상태로 둡니다.
 */
UCLASS()
class ENEMY_API UBTTask_WaitGameplayTagRemoved : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_WaitGameplayTagRemoved();

	// 노드가 실행될 때 (진입)
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// 매 프레임 체크 (Tick)
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
	// 에디터에서 기다릴 태그를 선택하는 변수
	UPROPERTY(EditAnywhere, Category = "GAS")
	FGameplayTag TagToWait;
};