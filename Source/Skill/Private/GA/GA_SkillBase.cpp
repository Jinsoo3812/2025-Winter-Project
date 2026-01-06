// Fill out your copyright notice in the Description page of Project Settings.

#include "GA/GA_SkillBase.h"
#include "Interface/ISkillManagerProvider.h"
#include "SkillManagerComponent.h"
#include "AbilitySystemComponent.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Skill, "Skill");
UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Casting, "State.Busy");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage, "Data.Skill.Damage");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Cooldown, "Data.Skill.Cooldown");

UGA_SkillBase::UGA_SkillBase()
{
	// 객체 생성 정책: GA를 소유한 액터마다 '하나의 GA 객체만' 생성
	// 메모리 효율적이며, 상태를 액터별로 관리할 수 있음
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 서버-클라이언트 통신 정책: 클라이언트의 입력이 발생하면, 서버의 응답을 기다리지 않고 즉시(예측) 실행
	// 네트워크 지연 시에도 반응성이 좋음
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer NewAbilityTags;
	NewAbilityTags.AddTag(TAG_Skill);
	SetAssetTags(NewAbilityTags);

	// CancelAbilitiesWithTag는 "다른 GA의 ASC에 붙은 Tag"를 검사
	// 자신이 Skill 임을 표시
	CancelAbilitiesWithTag.AddTag(TAG_Skill);

	// ActivationBlockedTags는 "시전자의 ASC에 붙은 Tag"를 검사
	// 시전자가 다른 GA를 시전 중일 때는 시전되지 않도록 설정
	ActivationBlockedTags.AddTag(TAG_Skill_Casting);
}

USkillManagerComponent* UGA_SkillBase::GetSkillManagerFromAvatar() const
{
	// 캐싱된 SkillManager가 유효하면 재사용 (성능 최적화)
	if (CachedSkillManager.IsValid())
	{
		return CachedSkillManager.Get();
	}

	// Avatar 가져오기
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		UE_LOG(LogTemp, Warning, TEXT("UGA_SkillBase::GetSkillManagerFromAvatar: Avatar is null"));
		return nullptr;
	}

	// ISkillManagerProvider 인터페이스를 구현했는지 확인
	ISkillManagerProvider* Provider = Cast<ISkillManagerProvider>(Avatar);
	if (Provider)
	{
		// 인터페이스를 통해 SkillManager 가져오기 (캐싱된 값)
		USkillManagerComponent* SkillManager = Provider->GetSkillManager();
		if (SkillManager)
		{
			// 다음 호출을 위해 캐싱
			CachedSkillManager = SkillManager;
			return SkillManager;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UGA_SkillBase::GetSkillManagerFromAvatar: Avatar does not implement ISkillManagerProvider or SkillManager is null"));
	return nullptr;
}

float UGA_SkillBase::GetRuneModifiedDamage() const
{
	// 현재 이 스킬이 장착된 슬롯 번호(InputID) 가져오기
	int32 SlotIndex = GetCurrentAbilitySpec()->InputID;

	// SkillManager 가져오기
	USkillManagerComponent* SkillManager = GetSkillManagerFromAvatar();
	if (!SkillManager)
	{
		return BaseDamage; // 매니저가 없으면 기본 데미지 반환
	}

	// 룬 배율 가져오기
	float Multiplier = SkillManager->GetTotalDamageMultiplier(SlotIndex);

	// 최종 데미지 반환
	// 플레이어의 공격력과도 합쳐진 식이 필요하지만, 현재는 스킬 자체의 데미지와 룬 배율만 적용
	return BaseDamage * Multiplier;
}

float UGA_SkillBase::GetRuneModifiedRange() const
{
	int32 SlotIndex = GetCurrentAbilitySpec()->InputID;

	// SkillManager 가져오기
	USkillManagerComponent* SkillManager = GetSkillManagerFromAvatar();
	if (!SkillManager)
	{
		return BaseRange; // 매니저가 없으면 기본 범위 반환
	}

	float Multiplier = SkillManager->GetTotalRangeMultiplier(SlotIndex);

	return BaseRange * Multiplier;
}

float UGA_SkillBase::GetRuneModifiedCooldown() const
{
	int32 SlotIndex = GetCurrentAbilitySpec()->InputID;

	// SkillManager 가져오기
	USkillManagerComponent* SkillManager = GetSkillManagerFromAvatar();
	if (!SkillManager)
	{
		return BaseCooldown; // 매니저가 없으면 기본 쿨타임 반환
	}

	// 쿨타임 감소율 (예: 0.1 = 10%)
	float Reduction = SkillManager->GetTotalCooldownReduction(SlotIndex);

	// 최종 쿨타임 = 기본 쿨타임 * (1 - 감소율)
	return BaseCooldown * (1.0f - Reduction);
}

// 데미지 처리: SetByCaller로 데미지 수치 주입
FGameplayEffectSpecHandle UGA_SkillBase::MakeRuneDamageEffectSpec(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!DamageEffect) {
		UE_LOG(LogTemp, Warning, TEXT("UGA_SkillBase::MakeRuneDamageEffectSpec: DamageEffect is not set"));
		return FGameplayEffectSpecHandle();
	}

	// Context 생성
	FGameplayEffectContextHandle Context = MakeEffectContext(Handle, ActorInfo);

	// Spec 생성
	FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(DamageEffect, GetAbilityLevel(), Context);

	if (SpecHandle.Data.IsValid())
	{
		// 룬 적용된 데미지 계산
		float FinalDamage = GetRuneModifiedDamage();

		// SetByCaller 태그(Data.Damage)에 수치 주입
		SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Damage, FinalDamage);
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("UGA_SkillBase::MakeRuneDamageEffectSpec: Failed to create SpecHandle"));
		return FGameplayEffectSpecHandle();
	}

	return SpecHandle;
}

// 쿨타임 처리: CommitAbility 시 자동으로 호출되는 함수 오버라이드
void UGA_SkillBase::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE) {
		UE_LOG(LogTemp, Warning, TEXT("UGA_SkillBase::ApplyCooldown: CooldownGE is not set"));
		return;
	}

	// 1. 쿨타임용 Spec 생성
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());

	if (SpecHandle.Data.IsValid())
	{
		// 룬 적용된 쿨타임 계산
		float FinalDuration = GetRuneModifiedCooldown();

		// SetByCaller 태그(Data.Cooldown)에 수치 주입
		SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Cooldown, FinalDuration);

		// 스킬별 고유 쿨타임 태그
		SpecHandle.Data->DynamicGrantedTags.AppendTags(UniqueCooldownTags);

		// 적용
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("UGA_SkillBase::ApplyCooldown: Failed to create Cooldown SpecHandle"));
	}
}

void UGA_SkillBase::NotifySkillCastStarted()
{
	// GA의 시전자에게 "State.Busy" 태그 추가
	// 시전 중 다른 스킬 시전이나 행동을 제한하기 위함
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar)
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		if (ASC)
		{
			// Loose 태그로 추가 (중첩형이 아닌 일반 태그)
			ASC->AddLooseGameplayTag(TAG_Skill_Casting);
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("UGA_SkillBase::NotifySkillCastStarted: Avatar is null"));
	}
}

void UGA_SkillBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// GA 종료 시 "State.Busy" 태그 제거
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar)
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		if (ASC && ASC->HasMatchingGameplayTag(TAG_Skill_Casting))
		{
			ASC->RemoveLooseGameplayTag(TAG_Skill_Casting);
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("UGA_SkillBase::EndAbility: Avatar is null"));
	}
	// 부모 클래스의 EndAbility 호출
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}