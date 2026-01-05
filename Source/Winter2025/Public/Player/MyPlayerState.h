#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "PlayerAttributeSet.h" // 경로가 안 맞으면 "Player/PlayerAttributeSet.h" 확인
#include "MyPlayerState.generated.h"

UCLASS()
class WINTER2025_API AMyPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMyPlayerState();

	// [인터페이스 구현]
	// 외부 시스템(다른 캐릭터, 게임플레이 큐 등)이 "이 사람 ASC 가지고 있나요?" 물어볼 때 답하는 함수입니다.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// AttributeSet(체력/마나 등)에 접근하기 위한 헬퍼 함수
	UPlayerAttributeSet* GetAttributeSet() const { return AttributeSet; }

protected:
	// 이제 ASC와 AttributeSet은 캐릭터가 아니라 PlayerState가 관리합니다.
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	UAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "GAS")
	UPlayerAttributeSet* AttributeSet;
};