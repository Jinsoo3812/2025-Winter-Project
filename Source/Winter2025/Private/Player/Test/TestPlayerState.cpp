// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Test/TestPlayerState.h"
#include "AbilitySystemComponent.h"
#include "SkillManagerComponent.h"
#include "Player/PlayerAttributeSet.h"
#include "Rune/DA_Rune.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

ATestPlayerState::ATestPlayerState()
{
	// ASC 생성 및 설정
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	
	// ASC의 서버-클라이언트 간의 복제화를 활성화
	AbilitySystemComponent->SetIsReplicated(true);
	
	// Replication Mode 설정
	// Minimal: GameplayEffect의 부여된 GameplayTag와 Attribute만 복제화
	// Mixed: 소유한 클라이언트(본인)에게는 모든 GameplayEffect를 복제화, 다른 클라이언트(다른 사람)에게는 Minimal
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// 스킬 매니저 컴포넌트 생성
	SkillManager = CreateDefaultSubobject<USkillManagerComponent>(TEXT("SkillManagerComponent"));

	// AttributeSet 생성 (체력, 최대체력 등의 수치 관리)
	// PlayerState에서 소유하므로 플레이어 죽었다 부활해도 유지됨
	AttributeSet = CreateDefaultSubobject<UPlayerAttributeSet>(TEXT("AttributeSet"));

	// 서버가 PlayerState의 상태를 클라이언트로 보내주는 주기(per second)
	// 기본값보다 높게 설정해 ASC의 변경사항이 빠르게 복제화될 수 있음
	SetNetUpdateFrequency(100.0f);

	// PlayerState의 데이터가 바뀌지 않는 동안 최소한 얼마의 주기로 업데이트 빈도를 유지
	// 아무 일도 일어나지 않는 동안 Min 빈도로 업데이트 유지
	SetMinNetUpdateFrequency(2.0f);
}

void ATestPlayerState::BeginPlay()
{
	Super::BeginPlay();

	// 플레이어 초기 스탯 로드
	InitializePlayerStats(PlayerLevel);

	// [추가] 게임 시작 시 Player 태그 부여 (서버 권한 확인 후 수행)
	if (HasAuthority())
	{
		if (AbilitySystemComponent)
		{
			// "Player"라는 이름의 태그를 요청하여 추가
			// (만약 별도로 정의해둔 매크로 TAG_Player가 있다면 그것으로 대체 가능)
			AbilitySystemComponent->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("Player")));

			UE_LOG(LogTemp, Log, TEXT("ATestPlayerState: 'Player' tag added successfully."));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ATestPlayerState::BeginPlay: AbilitySystemComponent is null. Cannot add 'Player' tag."));
		}
	}
}

UAbilitySystemComponent* ATestPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

USkillManagerComponent* ATestPlayerState::GetSkillManager() const
{
	return SkillManager;
}

void ATestPlayerState::InitializePlayerStats(int32 Level)
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

void ATestPlayerState::InitializeSkills()
{
	if (!SkillManager || !AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("ATestPlayerState::InitializeSkills: SkillManager or AbilitySystemComponent is null"));
		return;
	}

	// SkillManager 초기화
	SkillManager->SkillManagerInitialize(AbilitySystemComponent);

	// DefaultSkillSets를 순회하며 각 슬롯 처리
	for (int32 SlotIndex = 0; SlotIndex < DefaultSkillSets.Num(); ++SlotIndex)
	{
		FSkillSlot& SkillSlot = DefaultSkillSets[SlotIndex];

		// 초록 룬 캐시 업데이트 (블루프린트에서 설정한 값 반영)
		SkillSlot.UpdateGreenRuneCache();

		// 장착할 GA 결정 (초록 룬 있으면 스킬 교체)
		TSubclassOf<UGameplayAbility> SkillToEquip = SkillSlot.EquippedSkill;

		// 캐시된 초록 룬 확인 (O(1) 접근)
		if (SkillSlot.EquippedGreenRune)
		{
			UDA_Rune* GreenRune = SkillSlot.EquippedGreenRune;

			// 초록룬의 OriginalSkillClass와 현재 장착된 스킬이 일치하는지 확인
			if (GreenRune->OriginalSkillClass && SkillToEquip && SkillToEquip->IsChildOf(GreenRune->OriginalSkillClass))
			{
				// ReplacementSkillClass가 설정되어 있는지 확인
				if (GreenRune->ReplacementSkillClass)
				{
					// 스킬 교체
					SkillToEquip = GreenRune->ReplacementSkillClass;
					UE_LOG(LogTemp, Log, TEXT("[Slot %d] Green Rune detected: Replacing %s with %s"),
						SlotIndex,
						*GreenRune->OriginalSkillClass->GetName(),
						*GreenRune->ReplacementSkillClass->GetName());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[Slot %d] Green Rune has no ReplacementSkillClass set!"), SlotIndex);
				}
			}
		}

		// 스킬 장착
		if (SkillToEquip)
		{
			SkillManager->EquipSkill(SlotIndex, SkillToEquip);
		}

		// 룬 장착
		for (int32 RuneSlotIndex = 0; RuneSlotIndex < SkillSlot.RuneSlots.Num(); ++RuneSlotIndex)
		{
			if (SkillSlot.RuneSlots[RuneSlotIndex].RuneAsset)
			{
				SkillManager->EquipRune(SlotIndex, RuneSlotIndex, SkillSlot.RuneSlots[RuneSlotIndex].RuneAsset);
			}
		}
	}
}