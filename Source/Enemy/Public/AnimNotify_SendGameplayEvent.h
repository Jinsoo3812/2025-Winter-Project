#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h" // 태그 쓰려면 필요
#include "AnimNotify_SendGameplayEvent.generated.h"

/**
 * [GAS 전용] 절대 안 씹히는 C++ 노티파이
 */
UCLASS()
class ENEMY_API UAnimNotify_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	// 에디터에서 노티파이 이름 예쁘게 보이게 하기
	virtual FString GetNotifyName_Implementation() const override;

	// 실제 노티파이가 지나갈 때 실행되는 함수
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	// 에디터에서 선택할 이벤트 태그 (예: Event.Montage.Telegraph)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FGameplayTag EventTag;
};