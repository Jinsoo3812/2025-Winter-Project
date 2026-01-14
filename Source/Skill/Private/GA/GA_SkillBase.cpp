// Fill out your copyright notice in the Description page of Project Settings.

#include "GA/GA_SkillBase.h"
#include "Interface/ISkillManagerProvider.h"
#include "Interface/IAttributeSetProvider.h"
#include "SkillManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Engine/OverlapResult.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Player, "Player");

UE_DEFINE_GAMEPLAY_TAG(TAG_Skill, "Skill");
UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Casting, "State.Casting");
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
	// Preview 상태의 스킬이 발동 중일 때는 다른 스킬로 취소할 수 있도록
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
		return BaseDamage; // 매니저가 없으면 기본 피해량 반환
	}

	// 캐릭터 공격력 가져오기
	float CharacterAttackPower = 0.0f;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar)
	{
		// IAttributeSetProvider 인터페이스를 구현했는지 확인
		IAttributeSetProvider* Provider = Cast<IAttributeSetProvider>(Avatar);
		if (Provider)
		{
			UAttributeSet* AttributeSet = Provider->GetAttributeSet();
			if (AttributeSet)
			{
				// AttackPower 속성 가져오기 (FGameplayAttribute 사용)
				static FProperty* AttackPowerProperty = FindFProperty<FProperty>(AttributeSet->GetClass(), FName("AttackPower"));
				if (AttackPowerProperty)
				{
					const FGameplayAttributeData* AttackPowerData = AttackPowerProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(AttributeSet);
					if (AttackPowerData)
					{
						CharacterAttackPower = AttackPowerData->GetCurrentValue();
					}
				}
			}
		}
	}

	// 룬 계수 가져오기
	float Multiplier = SkillManager->GetTotalDamageMultiplier(SlotIndex);

	// 최종 피해량 = (캐릭터 공격력 + 스킬 기본 피해량) * 룬 계수
	return (CharacterAttackPower + BaseDamage) * Multiplier;
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
	// 범위는 캐릭터 기본값이 없으므로 그대로 유지
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
		SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Damage, -FinalDamage);
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

void UGA_SkillBase::NotifySkillCastFinished()
{
	// 투사체 발사 후, 스킬 로직(기폭 대기 등)은 남아있지만 캐릭터의 행동 제약은 풀고 싶을 때 호출
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (Avatar)
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		if (ASC)
		{
			// 이미 태그가 있다면 제거 (중복 제거 방지를 위해 확인)
			if (ASC->HasMatchingGameplayTag(TAG_Skill_Casting))
			{
				ASC->RemoveLooseGameplayTag(TAG_Skill_Casting);
			}
			// 태그가 없다면 굳이 로그를 찍지 않고 넘어감 (정상적인 흐름일 수 있음)
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UGA_SkillBase::NotifySkillCastFinished: ASC is null"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UGA_SkillBase::NotifySkillCastFinished: Avatar is null"));
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

void UGA_SkillBase::FindBlocksInRange(TArray<ABlockBase*>& OutBlocks)
{
	// 결과 배열 초기화 (매 프레임 호출될 수 있으므로 비워줌)
	OutBlocks.Empty();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: World is null"));
		return;
	}

	// AvatarActor를 OwnerPawn으로 캐시 
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: OwnerPawn is null"));
		return;
	}

	FVector PlayerLocation = OwnerPawn->GetActorLocation();

	// 박스 형태의 오버랩 영역 생성 (XY: 반지름, Z: 위아래 범위)
	// 원래 원통형 범위를 사용하고 싶지만, 언리얼 엔진의 오버랩 함수는 박스 형태만 지원하므로 박스로 대체
	// 이후에 XY 거리로 필터링하여 원형 범위로 보정
	FCollisionShape OverlapBox = FCollisionShape::MakeBox(FVector(RangeXY, RangeXY, RangeZ));

	// 오버랩 결과를 저장할 배열
	TArray<FOverlapResult> OverlapResults;

	// 어떤 오브젝트 타입을 대상으로 충돌 쿼리를 수행할지 설정 (무엇을 찾을 지)
	FCollisionObjectQueryParams ObjectQueryParams;

	// 대부분의 경우 블록은 움직이지 않으므로 WorldStatic
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	// 추후 스킬 사용으로 블록이 움직이면, 그 위의 블록도 움직이는 등의 상황을 고려하여 WorldDynamic도 포함
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// 충돌 쿼리의 옵션 및 예외 설정 (어떻게 찾을 지)
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerPawn); // 플레이어 자신은 제외

	// 범위 내 블록만 검색 (언리얼 물리 엔진의 공간 분할 구조 활용)
	bool bOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		PlayerLocation,
		FQuat::Identity,
		ObjectQueryParams,
		OverlapBox,
		QueryParams
	);

	// 오버랩 된 것이 없으면 로그를 찍을 필요는 없지만 함수는 종료
	if (!bOverlap)
	{
		return;
	}

	// 검색된 블록들 처리
	// FOverlapResult는 가벼우므로 &로 가져옵니다.
	for (const FOverlapResult& Result : OverlapResults)
	{
		ABlockBase* Block = Cast<ABlockBase>(Result.GetActor());
		if (!Block)
		{
			continue;
		}

		FVector BlockLocation = Block->GetActorLocation();

		// XY 평면 거리 재확인 (박스가 사각형이므로 원형 범위로 한번 더 필터링)
		float DistanceXY = FVector::Dist2D(PlayerLocation, BlockLocation);

		// 거리가 범위 밖이라면 포함하지 않음
		if (DistanceXY > RangeXY)
		{
			continue;
		}

		// 결과 배열에 유효한 블록 추가
		OutBlocks.Add(Block);
	}
}

void UGA_SkillBase::BatchHighlightBlocks(const TArray<ABlockBase*>& Blocks, EBlockHighlightState State)
{
	for (ABlockBase* Block : Blocks)
	{
		if (Block)
		{
			Block->SetHighlightState(State);
		}
	}
}