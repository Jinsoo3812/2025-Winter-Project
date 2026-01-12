// Fill out your copyright notice in the Description page of Project Settings.

#include "SkillManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Rune/DA_Rune.h"

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

// Sets default values for this component's properties
USkillManagerComponent::USkillManagerComponent()
{
	// 이 컴포넌트는 Tick이 필요 없으므로 비활성화하여 성능 향상
	PrimaryComponentTick.bCanEverTick = false;

	// 기본 슬롯 개수를 3개로 설정 (블루프린트에서 변경 가능)
	SkillSlots.SetNum(3);
}


// Called when the game starts
void USkillManagerComponent::BeginPlay()
{
	Super::BeginPlay();
}


// Called every frame
void USkillManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USkillManagerComponent::SkillManagerInitialize(UAbilitySystemComponent* InAbilitySystemComponent)
{
	// ASC 참조를 캐싱
	CachedAbilitySystemComponent = InAbilitySystemComponent;

	if (!CachedAbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("USkillManagerComponent::SkillManagerInitialize: AbilitySystemComponent is null"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("USkillManagerComponent::SkillManagerInitialize: Successfully initialized with ASC"));
}

bool USkillManagerComponent::EquipSkill(int32 SlotIndex, TSubclassOf<UGameplayAbility> SkillClass)
{
	// 유효성 검사: ASC 확인
	if (!CachedAbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("USkillManagerComponent::EquipSkill: ASC is not initialized"));
		return false;
	}

	// 유효성 검사: 슬롯 인덱스 확인
	if (!IsValidSlotIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("USkillManagerComponent::EquipSkill: Invalid slot index %d"), SlotIndex);
		return false;
	}

	// 유효성 검사: 스킬 클래스 확인
	if (!SkillClass)
	{
		UE_LOG(LogTemp, Error, TEXT("USkillManagerComponent::EquipSkill: SkillClass is null"));
		return false;
	}

	// 해당 슬롯에 이미 스킬이 있다면 먼저 해제
	if (SkillSlots[SlotIndex].EquippedSkill)
	{
		UnequipSkill(SlotIndex);
	}

	// GA Spec 생성
	// @param SkillClass 장착할 GA 클래스
	// @param Level 1로 시작
	// @param InputID는 SlotIndex로 설정
	// @param Source_object 이 컴포넌트의 소유자(플레이어)
	FGameplayAbilitySpec AbilitySpec(SkillClass, 1, SlotIndex, GetOwner());

	// ASC에 GA 부여 (GiveAbility)
	// @param AbilitySpec 생성한 GA Spec
	// @return 부여된 GA의 SpecHandle
	FGameplayAbilitySpecHandle NewHandle = CachedAbilitySystemComponent->GiveAbility(AbilitySpec);

	// 슬롯에 스킬 정보 저장
	SkillSlots[SlotIndex].EquippedSkill = SkillClass;
	SkillSlots[SlotIndex].AbilityHandle = NewHandle;

	/*UE_LOG(LogTemp, Log, TEXT("USkillManagerComponent::EquipSkill: Equipped %s to slot %d"), 
		*SkillClass->GetName(), SlotIndex);*/

	return true;
}

bool USkillManagerComponent::UnequipSkill(int32 SlotIndex)
{
	// 유효성 검사: ASC 확인
	if (!CachedAbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("USkillManagerComponent::UnequipSkill: ASC is not initialized"));
		return false;
	}

	// 유효성 검사: 슬롯 인덱스 확인
	if (!IsValidSlotIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("USkillManagerComponent::UnequipSkill: Invalid slot index %d"), SlotIndex);
		return false;
	}

	// 해당 슬롯에 스킬이 없다면 성공 반환
	if (!SkillSlots[SlotIndex].EquippedSkill)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkillManagerComponent::UnequipSkill: Slot %d is already empty"), SlotIndex);
		return true;
	}

	// ASC에서 스킬 제거
	CachedAbilitySystemComponent->ClearAbility(SkillSlots[SlotIndex].AbilityHandle);

	// 슬롯 정보 초기화
	UE_LOG(LogTemp, Log, TEXT("USkillManagerComponent::UnequipSkill: Unequipped skill from slot %d"), SlotIndex);
	
	SkillSlots[SlotIndex].EquippedSkill = nullptr;

	// FGameplayAbilitySpecHandle()는 유효하지 않은 핸들을 반환
	// @return INDEX_NONE
	SkillSlots[SlotIndex].AbilityHandle = FGameplayAbilitySpecHandle();

	return true;
}

TArray<FSkillSlot> USkillManagerComponent::GetEquippedSkills() const
{
	// 현재 장착된 스킬 슬롯 배열을 반환
	return SkillSlots;
}

bool USkillManagerComponent::EquipRune(int32 SlotIndex, int32 RuneSlotIndex, UDA_Rune* RuneData)
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

bool USkillManagerComponent::UnequipRune(int32 SlotIndex, int32 RuneSlotIndex)
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

float USkillManagerComponent::GetTotalDamageMultiplier(int32 SlotIndex) const
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

float USkillManagerComponent::GetTotalCooldownReduction(int32 SlotIndex) const
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

float USkillManagerComponent::GetTotalRangeMultiplier(int32 SlotIndex) const
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

bool USkillManagerComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	// 슬롯 인덱스가 배열 범위 내에 있는지 확인
	return SkillSlots.IsValidIndex(SlotIndex);
}

bool USkillManagerComponent::EquipRuneByID(int32 SlotIndex, int32 RuneSlotIndex, FName RuneID)
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
