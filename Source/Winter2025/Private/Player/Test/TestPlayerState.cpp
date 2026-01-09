// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Test/TestPlayerState.h"
#include "AbilitySystemComponent.h"
#include "SkillManagerComponent.h"
#include "Player/PlayerAttributeSet.h"
#include "Rune/DA_Rune.h"
#include "GA/GA_Destruction.h"
#include "GA/GA_SpinDestruction.h"

ATestPlayerState::ATestPlayerState()
{
	// ASC 생성 및 설정
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	
	// ASC의 서버-클라이언트 상태 동기화를 활성화
	AbilitySystemComponent->SetIsReplicated(true);
	
	// Replication Mode 설정
	// Minimal: GameplayEffect를 제외한 GameplayTag와 Attribute만 동기화
	// Mixed: 소유한 클라이언트(본인)에게는 모든 GameplayEffect를 동기화, 다른 클라이언트(다른 유저)에게는 Minimal
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// 스킬 매니저 컴포넌트 생성
	SkillManager = CreateDefaultSubobject<USkillManagerComponent>(TEXT("SkillManagerComponent"));

	// AttributeSet 생성 (체력, 최대체력 등의 수치 관리)
	// PlayerState에서 관리하므로 플레이어가 죽어도 데이터가 유지됨
	AttributeSet = CreateDefaultSubobject<UPlayerAttributeSet>(TEXT("AttributeSet"));

	// 서버가 PlayerState의 상태를 클라이언트에 복제하는 주기(per second)
	// 기본값은 낮기 때문에 ASC의 변경사항이 느리게 동기화될 수 있음
	SetNetUpdateFrequency(100.0f);

	// PlayerState의 데이터가 변경되지 않을 때에는 대역폭 절약을 위해 업데이트 빈도를 낮춤
	// 아무 일도 일어나지 않을 수록 Min 값으로 점진적 감소
	SetMinNetUpdateFrequency(2.0f);
}

void ATestPlayerState::BeginPlay()
{
	Super::BeginPlay();
	// 추가적인 초기화 코드 작성 가능
}

UAbilitySystemComponent* ATestPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ATestPlayerState::InitializeSkills()
{
	if (!SkillManager || !AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("ATestPlayerState::InitializeSkills: SkillManager or AbilitySystemComponent is null"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("========== [SKILL INITIALIZATION] START =========="));

	// SkillManager 초기화
	SkillManager->SkillManagerInitialize(AbilitySystemComponent);

	// DefaultSkillSets를 순회하며 각 슬롯 설정
	for (int32 SlotIndex = 0; SlotIndex < DefaultSkillSets.Num(); ++SlotIndex)
	{
		FSkillSlot& SkillSlot = DefaultSkillSets[SlotIndex];

		UE_LOG(LogTemp, Warning, TEXT("[Slot %d] Processing skill slot..."), SlotIndex);
		UE_LOG(LogTemp, Log, TEXT("[Slot %d] Original EquippedSkill: %s"), 
			SlotIndex, SkillSlot.EquippedSkill ? *SkillSlot.EquippedSkill->GetName() : TEXT("None"));

		// 초록 룬 캐시 업데이트 (블루프린트에서 설정한 룬 반영)
		SkillSlot.UpdateGreenRuneCache();

		// 장착할 GA 결정 (초록 룬 감지 및 교체)
		TSubclassOf<UGameplayAbility> SkillToEquip = SkillSlot.EquippedSkill;

		// 캐싱된 초록 룬 확인 (O(1) 접근)
		if (SkillSlot.EquippedGreenRune)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Slot %d] ? Green Rune detected! Value=%.2f"), 
				SlotIndex, SkillSlot.EquippedGreenRune->RuneValue);
			
			// 초록 룬의 값이 1.0이고 기본 스킬이 GA_Destruction인 경우
			if (SkillSlot.EquippedGreenRune->RuneValue == 1.0f)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Slot %d] Green Rune value is 1.0!"), SlotIndex);
				
				// BP 클래스도 체크하기 위해 IsChildOf 사용
				if (SkillToEquip && SkillToEquip->IsChildOf(UGA_Destruction::StaticClass()))
				{
					// SpinDestructionSkillClass가 설정되어 있는지 확인
					if (SpinDestructionSkillClass)
					{
						// BP_GA_SpinDestruction으로 교체
						SkillToEquip = SpinDestructionSkillClass;
						UE_LOG(LogTemp, Error, TEXT("[Slot %d] === REPLACING %s -> %s ==="), 
							SlotIndex, 
							*SkillSlot.EquippedSkill->GetName(), 
							*SpinDestructionSkillClass->GetName());
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("[Slot %d] SpinDestructionSkillClass is not set! Please assign BP_GA_SpinDestruction in BP_TestPlayerState"), SlotIndex);
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[Slot %d] Base skill is NOT GA_Destruction or its child (it's %s), skipping replacement"), 
						SlotIndex, SkillToEquip ? *SkillToEquip->GetName() : TEXT("None"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[Slot %d] Green Rune value (%.2f) is not 1.0, no skill replacement"), 
					SlotIndex, SkillSlot.EquippedGreenRune->RuneValue);
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[Slot %d] No green rune in this slot"), SlotIndex);
		}

		// 스킬 장착
		if (SkillToEquip)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Slot %d] Equipping skill: %s"), SlotIndex, *SkillToEquip->GetName());
			SkillManager->EquipSkill(SlotIndex, SkillToEquip);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Slot %d] No skill to equip"), SlotIndex);
		}

		// 룬 장착
		for (int32 RuneSlotIndex = 0; RuneSlotIndex < SkillSlot.RuneSlots.Num(); ++RuneSlotIndex)
		{
			if (SkillSlot.RuneSlots[RuneSlotIndex].RuneAsset)
			{
				UE_LOG(LogTemp, Log, TEXT("[Slot %d] Equipping rune in RuneSlot[%d]: Tag=%s, Value=%.2f"), 
					SlotIndex, RuneSlotIndex,
					*SkillSlot.RuneSlots[RuneSlotIndex].RuneAsset->RuneTag.ToString(),
					SkillSlot.RuneSlots[RuneSlotIndex].RuneAsset->RuneValue);
				SkillManager->EquipRune(SlotIndex, RuneSlotIndex, SkillSlot.RuneSlots[RuneSlotIndex].RuneAsset);
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[Slot %d] ========== Slot processing complete =========="), SlotIndex);
	}

	UE_LOG(LogTemp, Error, TEXT("========== [SKILL INITIALIZATION] COMPLETE =========="));
}