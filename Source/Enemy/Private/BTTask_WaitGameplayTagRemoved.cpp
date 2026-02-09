#include "BTTask_WaitGameplayTagRemoved.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AIController.h"
#include "GameFramework/Character.h"

UBTTask_WaitGameplayTagRemoved::UBTTask_WaitGameplayTagRemoved()
{
	NodeName = "Wait for Tag Removal"; // 에디터에 표시될 이름
	bNotifyTick = true; // Tick을 켜서 매 프레임 태그를 감시하게 함
}

EBTNodeResult::Type UBTTask_WaitGameplayTagRemoved::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* ControlledPawn = AIController ? AIController->GetPawn() : nullptr;

	if (!ControlledPawn) return EBTNodeResult::Failed;

	// IAbilitySystemInterface를 통해 ASC 가져오기
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ControlledPawn);
	UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;

	if (!ASC) return EBTNodeResult::Failed;

	// 태그가 존재하면 -> 기다리기 시작 (InProgress 반환)
	if (ASC->HasMatchingGameplayTag(TagToWait))
	{
		return EBTNodeResult::InProgress;
	}

	// 태그가 없으면 -> 기다릴 필요 없음 (성공 처리)
	return EBTNodeResult::Succeeded;
}

void UBTTask_WaitGameplayTagRemoved::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* ControlledPawn = AIController ? AIController->GetPawn() : nullptr;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(ControlledPawn);
	UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;

	if (ASC)
	{
		// [핵심] 태그가 사라졌는지 확인
		if (!ASC->HasMatchingGameplayTag(TagToWait))
		{
			// 태그가 없어졌다면 대기 종료 (성공)
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
	}
	else
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
}