// Fill out your copyright notice in the Description page of Project Settings.

#include "TestCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameplayAbilitySpec.h"
#include "Abilities/GameplayAbility.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

ATestCharacter::ATestCharacter()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	
	// ASC의 서버-클라이언트 상태 동기화를 활성화합니다.
	AbilitySystemComponent->SetIsReplicated(true);

	// GE를 제외한 Tag와 Attributes만 동기화하도록 설정합니다. 부하를 줄이기 위해 AI 캐릭터에 주로 사용.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

UAbilitySystemComponent* ATestCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ATestCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{	
		if (IsValid(Controller))
		{
			AbilitySystemComponent->InitAbilityActorInfo(this, this);

			for (TSubclassOf<UGameplayAbility>& Ability : Abilities)
			{
				if (Ability)
				{
					// FGameplayAbilitySpec는 ASC에 부여된 GA의 정보를 담는 구조체
					// Ability: GA의 CDO.
					// Level: GA의 현재 레벨
					// InputID: 입력 바인딩 ID
					// SourceObject: GA를 부여한 객체
					FGameplayAbilitySpec AbilitySpec(Ability, 1, 0, this);
					AbilitySystemComponent->GiveAbility(AbilitySpec);
				}
				else {
					UE_LOG(LogTemp, Warning, TEXT("Ability class is null in %s"), *GetName());
				}
			}
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("Controller is invalid in %s"), *GetName());
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("AbilitySystemComponent is null in %s"), *GetName());
	}
}

void ATestCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input을 사용하기 위해 기본 InputComponent를 캐스팅합니다.
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// InputMappingContext 추가
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				if (InputMappingContext)
				{
					Subsystem->AddMappingContext(InputMappingContext, 0);
					UE_LOG(LogTemp, Log, TEXT("InputMappingContext added to %s"), *GetName());
				}
				else {
					UE_LOG(LogTemp, Warning, TEXT("InputMappingContext is null in %s"), *GetName());
				}
			}
		}

		// 어빌리티 입력 바인딩
		if (AbilityInputAction)
		{
			EnhancedInputComponent->BindAction(AbilityInputAction, ETriggerEvent::Triggered, this, &ATestCharacter::OnAbilityInputPressed);
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("AbilityInputAction is null in %s"), *GetName());
		}
	}
}

void ATestCharacter::OnAbilityInputPressed()
{
	if (AbilitySystemComponent)
	{
		// 시전 가능한 GA들의 목록
		TArray<FGameplayAbilitySpec> ActivatableAbilities = AbilitySystemComponent->GetActivatableAbilities();

		// 이미 시전된 GA들을 제외하고, 시전 가능한 첫 번째 GA를 시전
		for (FGameplayAbilitySpec& Spec : ActivatableAbilities)
		{
			if (Spec.Ability && !Spec.IsActive())
			{
				AbilitySystemComponent->TryActivateAbility(Spec.Handle);
				UE_LOG(LogTemp, Warning, TEXT("Ability Activated: %s"), *Spec.Ability->GetName());
				break;
			}
			else if (!Spec.Ability) {
				UE_LOG(LogTemp, Warning, TEXT("Ability is null in Spec in %s"), *GetName());
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("Ability already active: %s"), *Spec.Ability->GetName());
			}
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("AbilitySystemComponent is null in %s"), *GetName());
	}
}

