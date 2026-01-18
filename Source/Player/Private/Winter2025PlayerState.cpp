// Fill out your copyright notice in the Description page of Project Settings.

#include "Winter2025PlayerState.h"
#include "AbilitySystemComponent.h"
#include "Winter2025PlayerAttributeSet.h" 
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

AWinter2025PlayerState::AWinter2025PlayerState()
{
	// ASC 설정
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// AttributeSet
	AttributeSet = CreateDefaultSubobject<UWinter2025PlayerAttributeSet>(TEXT("AttributeSet"));

	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* AWinter2025PlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AWinter2025PlayerState::InitializePlayerStats(int32 Level)
{
	if (!PlayerStatsDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("ATestPlayerState::InitializePlayerStats: PlayerStatsDataTable is null. Using default values."));
		return;
	}

	// 데이터 테이블에서 레벨에 해당하는 스탯 가져오기
	FString ContextString(TEXT("Player Stats Lookup"));
	FName RowName = FName(*FString::FromInt(Level));
	FPlayerInitialStatsRow* StatsRow = PlayerStatsDataTable->FindRow<FPlayerInitialStatsRow>(RowName, ContextString);

	if (StatsRow && AttributeSet)
	{
		// AttributeSet에 초기 스탯 적용
		AttributeSet->SetHealth(StatsRow->InitialHealth);
		AttributeSet->SetMaxHealth(StatsRow->InitialMaxHealth);
		AttributeSet->SetMana(StatsRow->InitialMana);
		AttributeSet->SetMaxMana(StatsRow->InitialMaxMana);
		AttributeSet->SetAttackPower(StatsRow->InitialAttackPower);
		AttributeSet->SetMovementSpeed(StatsRow->InitialMovementSpeed);

		UE_LOG(LogTemp, Log, TEXT("ATestPlayerState: Loaded stats for Level %d - HP: %f, Mana: %f, AttackPower: %f, MovementSpeed: %f"),
			Level, StatsRow->InitialMaxHealth, StatsRow->InitialMaxMana, StatsRow->InitialAttackPower, StatsRow->InitialMovementSpeed);

		// [추가] 2. 실제 캐릭터 이동 속도도 초기값으로 맞춰주기
		// PlayerState의 Owner는 Controller일 수도 있고 Pawn일 수도 있어서 확인 필요
		APawn* Pawn = GetPawn();
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
			{
				CMC->MaxWalkSpeed = StatsRow->InitialMovementSpeed;
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ATestPlayerState::InitializePlayerStats: Could not find stats for Level %d"), Level);
	}
}
