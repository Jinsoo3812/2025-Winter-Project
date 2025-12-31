// Fill out your copyright notice in the Description page of Project Settings.

#include "SkillManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

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

bool USkillManagerComponent::ActivateSkillBySlot(int32 SlotIndex)
{
	// 유효성 검사: ASC 확인
	if (!CachedAbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("USkillManagerComponent::ActivateSkillBySlot: ASC is not initialized"));
		return false;
	}

	// 유효성 검사: 슬롯 인덱스 확인
	if (!IsValidSlotIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("USkillManagerComponent::ActivateSkillBySlot: Invalid slot index %d"), SlotIndex);
		return false;
	}

	// 해당 슬롯에 스킬이 없다면 실패
	if (!SkillSlots[SlotIndex].EquippedSkill)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkillManagerComponent::ActivateSkillBySlot: No skill equipped in slot %d"), SlotIndex);
		return false;
	}

	// TryActivateAbility (스킬 활성화 시도)
	// @param AbilityHandle 활성화할 GA의 핸들
	// @return 활성화 성공 여부
	// Tag, Cost, Cooldown을 검사하고 InstancingPolicy에 따라 실행
	bool bSuccess = CachedAbilitySystemComponent->TryActivateAbility(SkillSlots[SlotIndex].AbilityHandle);

	if (bSuccess)
	{
		// UE_LOG(LogTemp, Log, TEXT("USkillManagerComponent::ActivateSkillBySlot: Activated skill in slot %d"), SlotIndex);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("USkillManagerComponent::ActivateSkillBySlot: Failed to activate skill in slot %d"), SlotIndex);
	}

	return bSuccess;
}

TArray<FSkillSlot> USkillManagerComponent::GetEquippedSkills() const
{
	// 현재 장착된 스킬 슬롯 배열을 반환
	return SkillSlots;
}

bool USkillManagerComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	// 슬롯 인덱스가 배열 범위 내에 있는지 확인
	return SkillSlots.IsValidIndex(SlotIndex);
}

