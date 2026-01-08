#include "EnemyBase.h"
#include "AbilitySystemComponent.h"
#include "EnemyAttributeSet.h" 
#include "GameplayEffectExtension.h" // GE 관련 헤더

AEnemyBase::AEnemyBase()
{
	// 1. ASC 생성 (Minimal 모드: AI는 예측이 필요 없으므로 효율적)
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// 2. AttributeSet 생성
	Attributes = CreateDefaultSubobject<UEnemyAttributeSet>(TEXT("Attributes"));
}

UAbilitySystemComponent* AEnemyBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		// 1. GAS 초기화 (AI는 Owner=Avatar=Self)
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// 2. 초기 태그 부여 (에디터 설정값 적용)
		if (InitialGameplayTags.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTags(InitialGameplayTags);
		}

		// 3. (서버만) 스탯 초기화 및 스킬 부여
		// 클라이언트는 복제된 값을 받으므로 서버에서만 처리하면 됩니다.
		if (HasAuthority())
		{
			InitializeAttributes();
			GiveDefaultAbilities();
		}
	}
}

void AEnemyBase::InitializeAttributes()
{
	// 에디터에 할당된 GE가 있고, ASC가 유효하다면 적용
	if (AbilitySystemComponent && DefaultAttributeEffect)
	{
		// GE 컨텍스트 생성 (누가 누구에게 적용하는가?)
		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(this);

		// GE 스펙 생성 (적용할 효과의 명세서)
		FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DefaultAttributeEffect, 1.0f, EffectContext);

		if (SpecHandle.IsValid())
		{
			// 자신에게 적용 (ApplyGameplayEffectSpecToSelf)
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

void AEnemyBase::GiveDefaultAbilities()
{
	// 에디터에 설정된 스킬 목록 순회
	if (HasAuthority() && AbilitySystemComponent)
	{
		for (TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
		{
			if (AbilityClass)
			{
				// 스킬 부여 (GiveAbility)
				// Level 1로 부여, InputID는 AI라 보통 -1(없음) 사용
				FGameplayAbilitySpec Spec(AbilityClass, 1, -1);
				AbilitySystemComponent->GiveAbility(Spec);
			}
		}
	}
}