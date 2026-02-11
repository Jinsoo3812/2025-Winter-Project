#include "SkillComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "DA_Rune.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemBlueprintLibrary.h"

USkillComponent::USkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
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

			// 룬 태그 적용 
			for (int32 j = 0; j < SkillSlots[i].RuneSlots.Num(); ++j)
			{
				UDA_Rune* Rune = SkillSlots[i].RuneSlots[j].RuneAsset;
				if (Rune)
				{
					UpdateRuneTag(i, j, Rune);
				}
			}
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

	// 태그 업데이트 (기존 태그 제거 -> 새 태그 추가)
	UpdateRuneTag(SlotIndex, RuneSlotIndex, RuneData);

	return true;
}

bool USkillComponent::UnequipRune(int32 SlotIndex, int32 RuneSlotIndex)
{
	if (!SkillSlots.IsValidIndex(SlotIndex)) return false;

	// 태그 제거 및 데이터 nullptr 처리
	UpdateRuneTag(SlotIndex, RuneSlotIndex, nullptr);

	return true;
}

void USkillComponent::UpdateRuneTag(int32 SlotIndex, int32 RuneSlotIndex, UDA_Rune* NewRune)
{
	if (!CachedASC) return;

	FRuneSlot& TargetRuneSlot = SkillSlots[SlotIndex].RuneSlots[RuneSlotIndex];

	// 기존 룬이 있었다면 태그 제거
	if (TargetRuneSlot.CachedCombinedTag.IsValid())
	{
		CachedASC->RemoveLooseGameplayTag(TargetRuneSlot.CachedCombinedTag);
		TargetRuneSlot.CachedCombinedTag = FGameplayTag::EmptyTag;
	}

	// 데이터 교체
	TargetRuneSlot.RuneAsset = NewRune;

	//  룬이 있다면 태그 생성 및 부착
	if (NewRune)
	{
		FGameplayTag SlotTag = SkillSlots[SlotIndex].SlotTag;
		FGameplayTag RuneTypeTag = NewRune->RuneTag;

		// 태그 합성: "Skill.Slot.1" + "Rune.Red" -> "Skill.Slot.1.Rune.Red"
		FGameplayTag CombinedTag = CombineRuneTag(SlotTag, RuneTypeTag);

		if (CombinedTag.IsValid())
		{
			CachedASC->AddLooseGameplayTag(CombinedTag);
			TargetRuneSlot.CachedCombinedTag = CombinedTag;
		}
	}
}

FGameplayTag USkillComponent::CombineRuneTag(FGameplayTag SlotTag, FGameplayTag RuneTag) const
{
	// 슬롯 태그나 룬 태그가 유효하지 않으면 리턴
	if (!SlotTag.IsValid() || !RuneTag.IsValid()) return FGameplayTag::EmptyTag;

	// 문자열 합성 (예: "Skill.Slot.1" + "." + "Rune.Red")
	FString CombinedString = FString::Printf(TEXT("%s.%s"), *SlotTag.ToString(), *RuneTag.ToString());

	// 태그 요청 (없으면 에러가 날 수 있으므로, 미리 프로젝트 세팅이나 Native를 준비)
	// 2번째 인자는 에러 발생 시 자동 생성 여부
	return FGameplayTag::RequestGameplayTag(FName(*CombinedString), true);
}

void USkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USkillComponent, SkillSlots);
}