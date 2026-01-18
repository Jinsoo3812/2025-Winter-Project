// Fill out your copyright notice in the Description page of Project Settings.

#include "Winter2025Player.h"
#include "Winter2025PlayerState.h"
#include "SkillSystemInterface.h"
#include "PlayerInputConfig.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Winter2025PlayerAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"

AWinter2025Player::AWinter2025Player()
{
}

UAbilitySystemComponent* AWinter2025Player::GetAbilitySystemComponent() const
{
	return CachedASC;
}

void AWinter2025Player::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeGAS(); // 서버 측 초기화
}

void AWinter2025Player::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeGAS(); // 클라이언트 측 초기화
}

void AWinter2025Player::InitializeGAS()
{
	AWinter2025PlayerState* PS = GetPlayerState<AWinter2025PlayerState>();
	if (!PS) return;

	CachedASC = PS->GetAbilitySystemComponent();
	if (CachedASC)
	{
		CachedASC->InitAbilityActorInfo(PS, this);

		// 구체적인 컴포넌트 클래스(USkillComponent) 대신 인터페이스를 찾음
		// PlayerState에 붙어있는 컴포넌트들 중, 스킬 인터페이스를 구현한 놈을 찾아서 초기화
		TArray<UActorComponent*> Components;
		PS->GetComponents(Components);

		for (UActorComponent* Comp : Components)
		{
			// 인터페이스 캐스팅 (Cast 대신 InterfaceCast 사용 가능하지만, IWinter2025SkillInterface는 순수 가상이 아닐 수 있으므로 Cast 사용)
			if (ISkillSystemInterface* SkillInterface = Cast<ISkillSystemInterface>(Comp))
			{
				SkillInterface->InitializeSkillSystem(CachedASC);
				UE_LOG(LogTemp, Log, TEXT("Winter2025Player: Skill System Initialized via Interface."));
				break; // 하나만 있다고 가정
			}
		}

		CachedASC->GetGameplayAttributeValueChangeDelegate(
			UWinter2025PlayerAttributeSet::GetMovementSpeedAttribute()
		).AddUObject(this, &AWinter2025Player::OnMovementSpeedChanged);

		// (이하 사망 이벤트 바인딩 로직 유지)
	}
}

void AWinter2025Player::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput || !InputConfig) return;

	// 로컬 플레이어인 경우에만 Input Mapping Context(IMC) 추가
	// (AI나 서버 측 캐릭터는 입력 매핑이 필요 없으므로)
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			// (1) 기본 이동/점프용 IMC (우선순위 0)
			if (BasicMappingContext)
			{
				Subsystem->AddMappingContext(BasicMappingContext, 0);
				UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Basic Mapping Context added"));
			}

			// (2) 스킬용 IMC (우선순위 1)
			// 스킬 입력이 들어오면 우선 처리되도록 Priority를 1로 설정합니다.
			if (SkillMappingContext)
			{
				Subsystem->AddMappingContext(SkillMappingContext, 1);
				UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Skill Mapping Context added"));
			}
		}
	}

	// DataAsset에 정의된 모든 입력을 순회하며 바인딩
	for (const FPlayerInputAction& Action : InputConfig->AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			// 눌렀을 때
			EnhancedInput->BindAction(Action.InputAction, ETriggerEvent::Started, this,
				&AWinter2025Player::Input_AbilityTagPressed, Action.InputTag);

			// 뗐을 때
			EnhancedInput->BindAction(Action.InputAction, ETriggerEvent::Completed, this,
				&AWinter2025Player::Input_AbilityTagReleased, Action.InputTag);
		}
	}

	// 이동 (Move) 바인딩
	if (MoveAction.Get())
	{
		EnhancedInput->BindAction(MoveAction.Get(), ETriggerEvent::Triggered, this, &AWinter2025Player::Move);
	}

	// 점프 (Jump) 바인딩
	if (JumpAction.Get())
	{
		// ACharacter::Jump는 존재하지만, StopJump는 없습니다. StopJumping을 써야 합니다.
		EnhancedInput->BindAction(JumpAction.Get(), ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction.Get(), ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}

	// 좌클릭 바인딩
	if (LeftClickAction.Get())
	{
		// 임시; 빌드만 되도록 해놨음
		EnhancedInput->BindAction(LeftClickAction.Get(), ETriggerEvent::Started, this, &ACharacter::StopJumping);
	}
}

void AWinter2025Player::Input_AbilityTagPressed(FGameplayTag InputTag)
{
	if (!CachedASC) return;

	// 태그를 기반으로 AbilitySpec을 찾아 활성화 시도
	// (SkillComponent에서 GiveAbility 할 때 DynamicAbilityTags에 InputTag를 넣었으므로 찾을 수 있음)
	TArray<FGameplayAbilitySpec*> Specs;
	CachedASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(InputTag), Specs);

	for (FGameplayAbilitySpec* Spec : Specs)
	{
		if (Spec && Spec->Ability)
		{
			CachedASC->TryActivateAbility(Spec->Handle);
		}
	}
}

void AWinter2025Player::Input_AbilityTagReleased(FGameplayTag InputTag)
{
	// 필요 시 구현 (차징 스킬 등)
	// ASC->AbilityLocalInputReleased(...) 등은 InputID 기반이라 태그 기반에서는 
	// 직접 Spec을 찾아 CancelAbility 등을 호출하거나 별도 이벤트를 보내야 함.
}

void AWinter2025Player::HandleAttributeEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	if (EventTag == FGameplayTag::RequestGameplayTag("Event.Character.Death"))
	{
		// 사망 처리 (Ragdoll, UI 표시, 조작 불능 등)
		UE_LOG(LogTemp, Warning, TEXT("Character Died!"));
		DetachFromControllerPendingDestroy();
	}
}

void AWinter2025Player::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AWinter2025Player::OnMovementSpeedChanged(const FOnAttributeChangeData& Data)
{
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = Data.NewValue;
	}
}