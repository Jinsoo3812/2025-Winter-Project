// Fill out your copyright notice in the Description page of Project Settings.

#include "TestCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameplayAbilitySpec.h"
#include "Abilities/GameplayAbility.h"
//#include "InputAction.h"
//#include "InputActionValue.h"
//#include "InputMappingContext.h"
#include "SkillManagerComponent.h"

ATestCharacter::ATestCharacter()
{
	// ASC 생성 및 설정
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	
	// ASC의 서버-클라이언트 상태 동기화를 활성화합니다.
	AbilitySystemComponent->SetIsReplicated(true);

	// GE를 제외한 Tag와 Attributes만 동기화하도록 설정합니다. 부하를 줄이기 위해 AI 캐릭터에 주로 사용.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// 스킬 매니저 컴포넌트 생성
	SkillManager = CreateDefaultSubobject<USkillManagerComponent>(TEXT("SkillManagerComponent"));
}

UAbilitySystemComponent* ATestCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ATestCharacter::BeginPlay()
{
	// 부모(최종적으로 AActor)의 BeginPlay 호출
	// 그 내부 구현에서 ReceiveBeginPlay 호출
	// ReceiveBeginPlay는 BP의 BeginPlay와 같음
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		if (IsValid(Controller))
		{
			// InitAbilityActorInfo (ASC 초기화)
			// @param OwnerActor: 이 ASC가 속한 액터 (서버가 소유한 playerstate)
			// @param AvatarActor: 이 ASC가 제어하는 액터 (레벨에서 실제로 존재하는 캐릭터)
			AbilitySystemComponent->InitAbilityActorInfo(this, this);

			// 스킬 매니저에 ASC 참조 전달
			if (SkillManager)
			{
				SkillManager->SkillManagerInitialize(AbilitySystemComponent);
				UE_LOG(LogTemp, Log, TEXT("ATestCharacter: SkillManager initialized successfully"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("ATestCharacter: SkillManager is null"));
			}
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("ATestCharacter: Controller is invalid in %s"), *GetName());
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("ATestCharacter: AbilitySystemComponent is null in %s"), *GetName());
	}
}