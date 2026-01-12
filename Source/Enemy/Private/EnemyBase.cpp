#include "EnemyBase.h"
#include "AbilitySystemComponent.h"
#include "EnemyAttributeSet.h" 
#include "GameplayEffectExtension.h" // GE 관련 헤더
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/CapsuleComponent.h" // 캡슐 콜리전 설정용
#include "Components/SkeletalMeshComponent.h" // 메쉬 설정용
#include "AIController.h" // AI 컨트롤러 접근용
#include "BrainComponent.h" // AI 뇌(비헤이비어 트리) 정지용
#include "GameFramework/CharacterMovementComponent.h" // (혹시 이동 멈출 때 필요)

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

void AEnemyBase::OnAttackHit(AActor* TargetActor)
{
	if (!TargetActor || !AttackDamageEffect) return;

	// 1. 타겟도 GAS 시스템(ASC)을 가지고 있는지 확인
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (TargetASC)
	{
		// 2. GameplayEffectSpec(설계도) 생성
		// 여기서 레벨에 따른 데미지 계수 등을 설정할 수 있습니다.
		FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
		ContextHandle.AddSourceObject(this);

		FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(AttackDamageEffect, 1.0f, ContextHandle);

		if (SpecHandle.IsValid())
		{
			// 3. 데미지 적용!
			AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

			UE_LOG(LogTemp, Log, TEXT("[EnemyBase] Successfully applied damage to %s"), *TargetActor->GetName());
		}
	}
}

void AEnemyBase::Die()
{
	// 이미 죽었으면 무시
	if (GetLifeSpan() > 0.0f) return;

	UE_LOG(LogTemp, Warning, TEXT("[EnemyBase] %s has Died!"), *GetName());

	// 1. 충돌 끄기 (시체 밟고 지나가도록)
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 래그돌 쓸거면 QueryAndPhysics

	// 2. 사망 몽타주 재생
	if (DeadMontage)
	{
		PlayAnimMontage(DeadMontage);
	}

	// 3. AI 컨트롤러 정지 (BrainComponent 정지)
	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		AICon->BrainComponent->StopLogic("Died");
	}

	// 4. 일정 시간 뒤 액터 제거 (필요하다면)
	SetLifeSpan(5.0f);
}

float AEnemyBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// 만약 GAS를 안 쓰는 일반 무기로 맞았을 때도 GAS 체력을 깎고 싶다면?
	// 여기서 Self에게 '데미지 받는 GE'를 적용하는 로직을 추가하면 됩니다.
	// 하지만 보통은 때리는 쪽에서 GE를 쏘므로 비워둬도 됩니다.

	return ActualDamage;
}
