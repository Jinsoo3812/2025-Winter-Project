// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEventInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UGameplayEventInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Actor들이 GameplayEvent를 수신할 수 있도록 하는 인터페이스
 * ASC가 없더라도 내부 구현을 통해 GameplayEvent를 수신할 수 있다.
 */
class INTERFACE_API IGameplayEventInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/** 
	 * GameplayEvent를 수신하는 함수
	 * @param EventTag 발생한 GameplayEvent의 태그
	 * @param Payload 이벤트에 대한 추가 데이터
	 */
	virtual void HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData& Payload) = 0;
};
