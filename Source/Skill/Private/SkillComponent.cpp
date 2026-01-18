// Fill out your copyright notice in the Description page of Project Settings.

#include "SkillComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "InputGameplayTags.h"
#include "Rune/DA_Rune.h"

USkillComponent::USkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

void USkillComponent::InitializeSkillSystem(UAbilitySystemComponent* InASC)
{
	CachedASC = InASC;
	if (!CachedASC) return;

	UE_LOG(LogTemp, Log, TEXT("SkillComponent: Initialize via Interface. Loading Default Skills..."));

	for (int32 i = 0; i < DefaultSkillSets.Num(); ++i)
	{
		FSkillSlot& SkillSlot = DefaultSkillSets[i];

		// 1. Slot Tag 유효성 검사 (BP에서 설정을 깜빡했을 경우 대비)
		if (!SkillSlot.SlotTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("SkillComponent: Slot index %d has NO SlotTag! Skipping."), i);
			continue;
		}

		// 2. 룬 캐시 업데이트
		SkillSlot.UpdateGreenRuneCache();

		// 3. 장착할 스킬 결정 (기본값)
		TSubclassOf<UGameplayAbility> SkillToEquip = SkillSlot.EquippedSkill;

		// 4. 초록 룬(스킬 교체) 로직
		if (SkillSlot.EquippedGreenRune)
		{
			UDA_Rune* GreenRune = SkillSlot.EquippedGreenRune;
			if (GreenRune->OriginalSkillClass && SkillToEquip && SkillToEquip->IsChildOf(GreenRune->OriginalSkillClass))
			{
				if (GreenRune->ReplacementSkillClass)
				{
					SkillToEquip = GreenRune->ReplacementSkillClass;
				}
			}
		}

		// 5. EquipSkill 호출
		if (SkillToEquip)
		{
			EquipSkill(SkillSlot.SlotTag, SkillToEquip);
		}
	}
}

void USkillComponent::EquipSkill(FGameplayTag SlotTag, TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!CachedASC) return;
	if (!SlotTag.IsValid()) return;

	// 1. 기존 스킬 해제 (ActiveSkillHandles Map 사용)
	// 해당 태그(예: Slot1) 자리에 이미 핸들이 있다면 제거
	if (ActiveSkillHandles.Contains(SlotTag))
	{
		FGameplayAbilitySpecHandle OldHandle = ActiveSkillHandles[SlotTag];
		if (OldHandle.IsValid())
		{
			CachedASC->ClearAbility(OldHandle);
		}
		ActiveSkillHandles.Remove(SlotTag);
	}

	// 2. 새 스킬 부여
	if (AbilityClass)
	{
		GiveAbility(SlotTag, AbilityClass);
	}
}

void USkillComponent::GiveAbility(FGameplayTag SlotTag, TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!CachedASC || !AbilityClass) return;

	// [InputID 제거 이유]
	// InputID는 ASC 내부 배열 인덱스와 1:1 매핑되어 경직된 구조를 만듭니다.
	// 대신 Tag를 사용하여 "이 능력이 발동되어야 하는 상황(트리거)"을 명시합니다.

	// AbilitySpec 생성 (Level: 1, InputID: -1)
	FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);

	// [핵심] Spec에 "동적 태그"로 SlotTag를 붙입니다.
	// 이제 Player가 "Input.Ability.Slot1" 태그 이벤트를 보내면, 
	// ASC는 이 태그가 붙은 Spec을 찾아서 실행시킵니다.
	Spec.DynamicAbilityTags.AddTag(SlotTag);

	// ASC에 등록하고 영수증(Handle) 받기
	FGameplayAbilitySpecHandle Handle = CachedASC->GiveAbility(Spec);

	// Map에 저장 (나중에 교체/해제를 위해)
	ActiveSkillHandles.Add(SlotTag, Handle);
}