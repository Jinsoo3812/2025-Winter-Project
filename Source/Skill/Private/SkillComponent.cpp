// Fill out your copyright notice in the Description page of Project Settings.

#include "SkillComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Rune/DA_Rune.h"
#include "Net/UnrealNetwork.h"

USkillComponent::USkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;

	// 컴포넌트 자체의 리플리케이션 활성화
	SetIsReplicatedByDefault(true);
}

void USkillComponent::InitializeSkillSystem(UAbilitySystemComponent* InASC)
{
	CachedASC = InASC;
	if (!CachedASC) {
		UE_LOG(LogTemp, Warning, TEXT("SkillComponent: InitializeSkillSystem called with NULL ASC!"));
		return;
	}

	/*
	* BP에서 설정한 스킬 및 룬 세트를 장착
	* DefaultSkillSets를 항상 SkillSlots로 복사하여 사용
	* (런타임에 SkillSlots를 수정하면서 사용, DefaultSkillSets는 원본 유지)
	*/
	if (DefaultSkillSets.Num() > 0)
	{
		SkillSlots = DefaultSkillSets;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillComponent] DefaultSkillSets is empty! Please configure skills in Blueprint."));
		return;
	}

	// 2. 서버 권한이 있을 때만 로직 수행 (GAS 부여 및 데이터 갱신)
	if (GetOwner()->HasAuthority())
	{
		// SkillSlots 배열 하나만 순회하면 됩니다. (이중 루프 X)
		for (int32 i = 0; i < SkillSlots.Num(); ++i)
		{
			FSkillSlot& SkillSlot = SkillSlots[i]; // 참조로 가져와야 원본 수정 가능

			// Tag 유효성 검사
			if (!SkillSlot.SlotTag.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("SkillComponent: SkillSlot %d has invalid SlotTag!"), i);
				continue;
			}

			// 룬 캐시 업데이트
			SkillSlot.UpdateGreenRuneCache();

			// 장착할 스킬 결정 (기본값)
			TSubclassOf<UGameplayAbility> FinalSkillClass = SkillSlot.EquippedSkill;

			// 3. 초록 룬(스킬 교체) 적용 로직
			if (SkillSlot.EquippedGreenRune)
			{
				UDA_Rune* GreenRune = SkillSlot.EquippedGreenRune;

				// 상속 관계 확인 (안전장치)
				if (GreenRune->OriginalSkillClass && FinalSkillClass && FinalSkillClass->IsChildOf(GreenRune->OriginalSkillClass))
				{
					if (GreenRune->ReplacementSkillClass)
					{
						// 교체할 스킬로 변경
						FinalSkillClass = GreenRune->ReplacementSkillClass;

						// [중요] 실제 데이터(구조체)도 변경해줘야 리플리케이션을 통해 클라이언트 UI가 바뀜
						SkillSlot.EquippedSkill = FinalSkillClass;
					}
				}
			}

			// 4. 최종 결정된 스킬을 GAS에 등록
			if (FinalSkillClass)
			{
				EquipSkill(SkillSlot.SlotTag, FinalSkillClass);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[SkillComponent] Slot %d has no FinalSkillClass!"), i);
			}
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

	// SlotTag를 InputID로 변환 (Slot1 -> 0, Slot2 -> 1, Slot3 -> 2)
	int32 InputID = INDEX_NONE;
	
	// SkillSlots 배열에서 이 SlotTag의 인덱스 찾기
	for (int32 i = 0; i < SkillSlots.Num(); ++i)
	{
		if (SkillSlots[i].SlotTag == SlotTag)
		{
			InputID = i;
			break;
		}
	}

	if (InputID == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillComponent] Could not find slot index for tag: %s"), *SlotTag.ToString());
		return;
	}

	// AbilitySpec 생성 (InputID 설정!)
	FGameplayAbilitySpec Spec(AbilityClass, 1, InputID, this);

	// DynamicTag도 추가 (태그 기반 검색용)
	Spec.DynamicAbilityTags.AddTag(SlotTag);

	// ASC에 등록하고 영수증(Handle) 받기
	FGameplayAbilitySpecHandle Handle = CachedASC->GiveAbility(Spec);

	// Map에 저장 (나중에 교체/해제를 위해)
	ActiveSkillHandles.Add(SlotTag, Handle);
}

float USkillComponent::GetTotalDamageMultiplier(int32 SlotIndex) const
{
	if (!IsValidSlotIndex(SlotIndex)) {
		UE_LOG(LogTemp, Warning, TEXT("GetTotalDamageMultiplier: Invalid SlotIndex %d"), SlotIndex);
		return 1.0f; // 기본값 반환
	}

	// 기본 배율은 1.0 (100%)
	float TotalMultiplier = 1.0f;

	// 해당 스킬 슬롯에 장착된 3개의 룬을 순회
	for (const FRuneSlot& Slot : SkillSlots[SlotIndex].RuneSlots)
	{
		const UDA_Rune* Rune = Slot.RuneAsset;
		if (Rune && Rune->RuneTag == TAG_Rune_Red)
		{
			// 곱셈 방식으로 변경: RuneValue가 2.0이면 2배가 됨
			TotalMultiplier *= Rune->RuneValue;
		}
	}
	return TotalMultiplier;
}

float USkillComponent::GetTotalCooldownReduction(int32 SlotIndex) const
{
	if (!IsValidSlotIndex(SlotIndex)) {
		UE_LOG(LogTemp, Warning, TEXT("GetTotalCooldownReduction: Invalid SlotIndex %d"), SlotIndex);
		return 0.0f; // 기본값 반환
	}

	// 쿨타임 감소량 합계 (0.0에서 시작)
	float TotalReduction = 0.0f;

	for (const FRuneSlot& Slot : SkillSlots[SlotIndex].RuneSlots)
	{
		const UDA_Rune* Rune = Slot.RuneAsset;
		if (Rune && Rune->RuneTag == TAG_Rune_Yellow)
		{
			TotalReduction += Rune->RuneValue;
		}
	}

	// 최대 쿨감 제한 (예: 99% 이상 쿨감 방지)
	return FMath::Clamp(TotalReduction, 0.0f, 0.99f);
}

float USkillComponent::GetTotalRangeMultiplier(int32 SlotIndex) const
{
	if (!IsValidSlotIndex(SlotIndex)) {
		UE_LOG(LogTemp, Warning, TEXT("GetTotalRangeMultiplier: Invalid SlotIndex %d"), SlotIndex);
		return 1.0f; // 기본값 반환
	}

	// 기본 배율 1.0배
	float TotalMultiplier = 1.0f;

	for (const FRuneSlot& Slot : SkillSlots[SlotIndex].RuneSlots)
	{
		const UDA_Rune* Rune = Slot.RuneAsset;
		if (Rune && Rune->RuneTag == TAG_Rune_Blue)
		{
			// 곱셈 방식으로 변경: RuneValue가 2.0이면 2배가 됨
			TotalMultiplier *= Rune->RuneValue;
		}
	}

	return TotalMultiplier;
}

bool USkillComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	// 슬롯 인덱스가 배열 범위 내에 있는지 확인
	return SkillSlots.IsValidIndex(SlotIndex);
}

bool USkillComponent::EquipRuneByID(int32 SlotIndex, int32 RuneSlotIndex, FName RuneID)
{
	// 데이터 테이블 유효성 검사
	if (!RuneDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("SkillManagerComponent: RuneDataTable is null.Please assign it in the blueprint."));
		return false;
	}

	// 데이터 테이블에서 행(Row) 찾기
	// ContextString: 오류 메시지에 사용되는 문자열
	static const FString ContextString(TEXT("Rune Lookup"));
	FRuneDataRow* Row = RuneDataTable->FindRow<FRuneDataRow>(RuneID, ContextString);

	if (Row)
	{
		// 룬 에셋 유효성 검사
		if (Row->RuneAsset)
		{
			// 기존 EquipRune 함수 재사용
			return EquipRune(SlotIndex, RuneSlotIndex, Row->RuneAsset);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SkillManagerComponent: RuneAsset in Row '%s' is null."), *RuneID.ToString());
			return false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("EquipRuneByID: RuneID '%s' not found in DataTable."), *RuneID.ToString());
		return false;
	}
}

bool USkillComponent::EquipRune(int32 SlotIndex, int32 RuneSlotIndex, UDA_Rune* RuneData)
{
	// 1. 스킬 슬롯 유효성 검사
	if (!IsValidSlotIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipRune: Invalid SlotIndex %d"), SlotIndex);
		return false;
	}

	// 2. 룬 슬롯 인덱스 유효성 검사 (0~2)
	if (!SkillSlots[SlotIndex].RuneSlots.IsValidIndex(RuneSlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipRune: Invalid RuneSlotIndex %d"), RuneSlotIndex);
		return false;
	}

	// 3. 룬 데이터 유효성 검사
	if (!RuneData)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipRune: RuneData is null"));
		return false;
	}

	// 4. 초록 룬 중복 검사 및 제거
	if (RuneData->RuneTag == TAG_Rune_Green)
	{
		// 해당 스킬 슬롯의 모든 룬 슬롯을 순회하며 기존 초록 룬 제거
		for (int32 i = 0; i < SkillSlots[SlotIndex].RuneSlots.Num(); ++i)
		{
			// 현재 장착하려는 슬롯은 제외
			if (i != RuneSlotIndex)
			{
				UDA_Rune* ExistingRune = SkillSlots[SlotIndex].RuneSlots[i].RuneAsset;
				if (ExistingRune && ExistingRune->RuneTag == TAG_Rune_Green)
				{
					SkillSlots[SlotIndex].RuneSlots[i].RuneAsset = nullptr;
				}
			}
		}
	}

	// 5. 룬 장착 (배열의 해당 인덱스를 덮어씀)
	SkillSlots[SlotIndex].RuneSlots[RuneSlotIndex].RuneAsset = RuneData;

	// 초록 룬 캐시 업데이트
	SkillSlots[SlotIndex].UpdateGreenRuneCache();

	return true;
}

bool USkillComponent::UnequipRune(int32 SlotIndex, int32 RuneSlotIndex)
{
	if (!IsValidSlotIndex(SlotIndex) || !SkillSlots[SlotIndex].RuneSlots.IsValidIndex(RuneSlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UnequipRune: Invalid SlotIndex %d or RuneSlotIndex %d"), SlotIndex, RuneSlotIndex);
		return false;
	}

	// 해당 칸을 비움 (nullptr)
	SkillSlots[SlotIndex].RuneSlots[RuneSlotIndex].RuneAsset = nullptr;

	// 초록 룬 캐시 업데이트
	SkillSlots[SlotIndex].UpdateGreenRuneCache();

	return true;
}

// FSkillSlot의 UpdateGreenRuneCache 구현
void FSkillSlot::UpdateGreenRuneCache()
{
	EquippedGreenRune = nullptr;

	// 초록 룬 태그
	FGameplayTag GreenRuneTag = FGameplayTag::RequestGameplayTag(FName("Rune.Green"));

	// 모든 룬 슬롯을 확인하여 초록 룬 찾기
	for (const FRuneSlot& Slot : RuneSlots)
	{
		if (Slot.RuneAsset && Slot.RuneAsset->RuneTag.MatchesTagExact(GreenRuneTag))
		{
			EquippedGreenRune = Slot.RuneAsset;
			break; // 초록 룬은 하나만 가능하므로 찾으면 중단
		}
	}
}

void USkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// SkillSlots 배열을 서버 -> 클라이언트로 복제
	DOREPLIFETIME(USkillComponent, SkillSlots);
}