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

	// IAbilitySystemInterface
	UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UWinter2025PlayerAttributeSet* GetAttributeSet() const { return AttributeSet; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UWinter2025PlayerAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	UDataTable* PlayerStatsDataTable;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	int32 PlayerLevel = 1;

	void InitializePlayerStats(int32 Level);
};