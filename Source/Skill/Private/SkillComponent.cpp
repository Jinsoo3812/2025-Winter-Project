#include "SkillComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemBlueprintLibrary.h"

USkillComponent::USkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void USkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USkillComponent, SkillSlots);
}

void USkillComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	UAbilitySystemComponent* FoundASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);

	if (FoundASC)
	{
		InitializeSkillSystem(FoundASC);
	}
	else
	{
		// 이 오류 로그 터진거면 PlayerState의 OnPossessed/OnRep_PlayerState에서 따로 호출해줘야 함.
		UE_LOG(LogTemp, Warning, TEXT("SkillComponent: Cannot find ASC in Owner Actor %s"), *Owner->GetName());
	}
}

void USkillComponent::InitializeSkillSystem(UAbilitySystemComponent* InASC)
{
	CachedASC = InASC;
	if (!CachedASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillComponent: Missing ASC inside InitializeSkillSystem"));
		return;
	}

	// 1. DefaultSkills를 런타임 SkillSlots로 복사 (테스트용)
	if (DefaultSkills.Num() > 0)
	{
		SkillSlots = DefaultSkills;
	}
	// 없으면 기본 에디터 상 기본 개수
	else
	{
		SkillSlots.SetNum(SkillSlotCount);
	}

	// 룬 슬롯 개수 초기화
	for (FSkillSlot& Slot : SkillSlots)
	{
		if (Slot.RuneSlots.Num() != RuneSlotCount)
		{
			Slot.RuneSlots.SetNum(RuneSlotCount);
		}
	}

	// 2. 서버 권한이 있을 때 스킬 및 룬 태그 적용
	if (GetOwner()->HasAuthority())
	{
		for (int32 i = 0; i < SkillSlots.Num(); ++i)
		{
			// 스킬 등록
			EquipSkill(SkillSlots[i].SlotTag, SkillSlots[i].EquippedSkill);
		}
	}
}

void USkillComponent::EquipSkill(FGameplayTag SlotTag, TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!CachedASC || !SlotTag.IsValid() || !AbilityClass)
	{
		if (!CachedASC)
		{
			UE_LOG(LogTemp, Warning, TEXT("SkillComponent: Cannot equip skill, ASC is null"));
		}
		if (!SlotTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("SkillComponent: Cannot equip skill, SlotTag is invalid"));
		}
		if (!AbilityClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("SkillComponent: Cannot equip skill, AbilityClass is null"));
		}
		return;
	}

	// 기존 스킬 해제
	if (ActiveSkillHandles.Contains(SlotTag))
	{
		FGameplayAbilitySpecHandle OldHandle = ActiveSkillHandles[SlotTag];
		if (OldHandle.IsValid())
		{
			CachedASC->ClearAbility(OldHandle);
		}
		ActiveSkillHandles.Remove(SlotTag);
	}

	// 입력 ID 매핑 (SlotTag -> Index)
	int32 InputID = INDEX_NONE;
	for (int32 i = 0; i < SkillSlots.Num(); ++i)
	{
		if (SkillSlots[i].SlotTag == SlotTag)
		{
			InputID = i;
			break;
		}
	}

	// 새 스킬 스펙 생성
	FGameplayAbilitySpec Spec(AbilityClass, 1, InputID, this);

	// 스킬 GA 본인이 몇 번 슬롯인지 알 수 있도록 태그 부착
	Spec.GetDynamicSpecSourceTags().AddTag(SlotTag);

	FGameplayAbilitySpecHandle Handle = CachedASC->GiveAbility(Spec);
	ActiveSkillHandles.Add(SlotTag, Handle);
}

bool USkillComponent::EquipRune(int32 SlotIndex, int32 RuneSlotIndex, UDA_Rune* RuneData)
{
	if (!SkillSlots.IsValidIndex(SlotIndex)) return false;
	if (!SkillSlots[SlotIndex].RuneSlots.IsValidIndex(RuneSlotIndex)) return false;
	if (!RuneData) return false;

	SkillSlots[SlotIndex].RuneSlots[RuneSlotIndex] = RuneData;

	return true;
}

bool USkillComponent::UnequipRune(int32 SlotIndex, int32 RuneSlotIndex)
{
	if (!SkillSlots.IsValidIndex(SlotIndex)) return false;

	SkillSlots[SlotIndex].RuneSlots[RuneSlotIndex] = nullptr;

	return true;
}

float USkillComponent::GetTotalRuneMultiplier(FGameplayTag SlotTag, ERuneType Type) const
{
	if (!SlotTag.IsValid()) return 1.0f;

	// 1. 해당 태그를 가진 슬롯 찾기
	const FSkillSlot* FoundSlot = SkillSlots.FindByPredicate([&](const FSkillSlot& Slot) {
		return Slot.SlotTag == SlotTag;
		});

	if (!FoundSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillComponent: GetTotalRuneMultiplier - Cannot find SkillSlot with Tag %s"), *SlotTag.ToString());
		return 1.0f;
	}

	// 2. 해당 타입의 룬 개수 카운트
	int32 RuneCount = 0;
	for (const TObjectPtr<UDA_Rune>& RunePtr : FoundSlot->RuneSlots)
	{
		if (RunePtr && RunePtr->RuneType == Type)
		{
			RuneCount++;
		}
	}

	if (RuneCount <= 0 || !RuneCurveTable) return 1.0f;

	// 1. 룬 타입에 맞는 Row Name 결정 (테이블의 행 이름과 일치해야 함)
	FName RowName = NAME_None;
	switch (Type)
	{
	case ERuneType::Red:
		RowName = FName("Damage"); // CurveTable의 행 이름
		break;
	case ERuneType::Yellow:
		RowName = FName("Cooldown");
		break;
	case ERuneType::Blue:
		RowName = FName("Range");
		break;
	default:
		return 1.0f;
	}

	if (RuneCount <= 0 || !RuneCurveTable) return 1.0f;

	const FRealCurve* Curve = RuneCurveTable->FindCurve(RowName, FString(), false);

	if (Curve)
	{
		return Curve->Eval((float)RuneCount);
	}

	return 1.0f;
}