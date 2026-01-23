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
#include "GameplayTagsManager.h" // 태그 관리를 위해 추가


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

		// StartupAbilities에 등록된 스킬들을 실제로 부여
		// (서버에서만 실행해야 함)
		if (HasAuthority())
		{
			for (TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
			{
				if (AbilityClass)
				{
					// 레벨 1짜리 스킬 생성 및 부여
					FGameplayAbilitySpec Spec(AbilityClass, 1);
					AbilitySystemComponent->GiveAbility(Spec);
				}
			}
		}
	}
}

void AEnemyBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// GAS 시스템 초기화 (AI가 컨트롤러에 빙의될 때 실행)
	if (AbilitySystemComponent)
	{
		// 1. Owner와 Avatar 설정 (서버/클라이언트 동기화 중요)
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// 2. 초기 스탯 및 스킬 부여 (서버 권한이 있을 때만)
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
	// 이미 스킬이 부여되었는지 확인하여 중복 방지
	if (!HasAuthority() || !AbilitySystemComponent || bAbilitiesInitialized) return;

	for (TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (AbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, -1));
		}
	}
	bAbilitiesInitialized = true; // 플래그 설정
}


/*
* ForceResetCombatState
* 전투 상태를 강제로 초기화합니다.
* - 모든 스킬 및 어빌리티를 취소
* - 이동 정지 및 AI 로직 초기화
* - 어그로 초기화 (필요시 추가 구현 가능)
*/
void AEnemyBase::ForceResetCombatState()
{
	// 1. GAS 어빌리티(스킬/공격) 모두 취소
	if (GetAbilitySystemComponent())
	{
		GetAbilitySystemComponent()->CancelAllAbilities();
	}

	// 2. 이동 및 AI 로직 정지
	if (AController* CurrentController = GetController())
	{
		CurrentController->StopMovement(); // 물리적 이동 멈춤

		if (AAIController* AIC = Cast<AAIController>(CurrentController))
		{
			AIC->ClearFocus(EAIFocusPriority::Gameplay); // 쳐다보던 것 멈춤
			// 필요한 경우 BrainComponent(BT)를 잠시 멈출 수도 있음
			// AIC->GetBrainComponent()->StopLogic("PhaseTransition"); 
		}
	}

	// 3. (선택사항) 물리 상태 초기화 (공중에 떠있거나 밀려나는 중이라면 멈춤)
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
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

			UE_LOG(LogTemp, Log, TEXT("[EnemyBase] : Successfully applied damage to %s"), *TargetActor->GetName());
		}
	}
}

void AEnemyBase::Die()
{
	// 1. 이미 죽었거나 파괴 중이면 중단 (중복 실행 방지 핵심)
	if (bIsDying || !IsValid(this)) return;
	bIsDying = true; // 헤더에 bool bIsDying; 선언 필요

	// 2. 서버에서 사망 태그 부여
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	}

	// 3. 컴포넌트 유효성 검사 후 콜리전 제거 
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (GetMesh())
	{
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 4. AI 컨트롤러 및 브레인 컴포넌트 안전하게 정지 
	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (AICon->BrainComponent)
		{
			AICon->BrainComponent->StopLogic("Died");
		}
	}

	// 5. 블루프린트 이벤트 호출 및 생존 기간 설정 
	BP_OnDie();
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
