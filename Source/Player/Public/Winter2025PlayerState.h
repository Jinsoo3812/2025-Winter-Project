// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Winter2025PlayerState.generated.h"

class UAbilitySystemComponent;
class UWinter2025PlayerAttributeSet;

UCLASS()
class AWinter2025PlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AWinter2025PlayerState();

	/* IAbilitySystemInterface 구현 */
	UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/* Player AttributeSet Getter */
	UWinter2025PlayerAttributeSet* GetAttributeSet() const { return AttributeSet; }

	/* 플레이어 기본 스탯 초기화 */
	void InitializePlayerStats(int32 Level);

protected:
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UWinter2025PlayerAttributeSet> AttributeSet;

	/* 레벨 별 플레이어 기본 스탯 */
	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	UDataTable* PlayerStatsDataTable;

	/* 현재 플레이어 레벨 */
	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	int32 PlayerLevel = 1;
};