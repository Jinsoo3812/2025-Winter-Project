// Fill out your copyright notice in the Description page of Project Settings.

#include "Winter2025Player.h"
#include "Winter2025PlayerState.h"
#include "SkillSystemInterface.h"
#include "PlayerInputConfig.h"
#include "InputGameplayTags.h"
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

	// 권한을 가진 서버에서만 플레이어 스탯 초기화
	if (AWinter2025PlayerState* PS = GetPlayerState<AWinter2025PlayerState>())
	{
		// 예: 1레벨로 초기화 (나중에 저장된 레벨을 불러올 수도 있음)
		PS->InitializePlayerStats(1);
	}
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
		// ASC 초기화
		CachedASC->InitAbilityActorInfo(PS, this);

		// 구체적인 컴포넌트 클래스(USkillComponent) 대신 인터페이스를 찾음
		// PlayerState에 붙어있는 컴포넌트들 중, 스킬 인터페이스를 구현한 놈을 찾아서 초기화
		TArray<UActorComponent*> Components;
		PS->GetComponents(Components);

		for (UActorComponent* Comp : Components)
		{
			// SkillComponent 초기화
			if (ISkillSystemInterface* SkillInterface = Cast<ISkillSystemInterface>(Comp))
			{
				SkillInterface->InitializeSkillSystem(CachedASC);
				break; // 하나만 있다고 가정
			}
		}

		CachedASC->GetGameplayAttributeValueChangeDelegate(
			UWinter2025PlayerAttributeSet::GetMovementSpeedAttribute()
		).AddUObject(this, &AWinter2025Player::OnMovementSpeedChanged);

	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Winter2025Player: Failed to get AbilitySystemComponent from PlayerState."));
	}
}

void AWinter2025Player::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput || !InputConfig) {
		UE_LOG(LogTemp, Warning, TEXT("AWinter2025Player: EnhancedInputComponent or InputConfig is null."));
		return;
	}

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
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("AWinter2025Player: Basic Mapping Context is null"));
			}

			// (2) 스킬용 IMC (우선순위 1)
			// 스킬 입력이 들어오면 우선 처리되도록 Priority를 1로 설정합니다.
			if (SkillMappingContext)
			{
				Subsystem->AddMappingContext(SkillMappingContext, 1);
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("AWinter2025Player: Skill Mapping Context is null"));
			}
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("AWinter2025Player: Failed to get EnhancedInputLocalPlayerSubsystem."));
			return;
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("AWinter2025Player: Controller is not a PlayerController."));
	}

	// DataAsset에 정의된 모든 입력을 순회하며 바인딩
	UE_LOG(LogTemp, Warning, TEXT("[Setup] Binding %d Ability Input Actions"), InputConfig->AbilityInputActions.Num());
	for (const FPlayerInputAction& Action : InputConfig->AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Setup] Binding InputAction: %s to Tag: %s"), 
				*Action.InputAction->GetName(), *Action.InputTag.ToString());

			// 눌렀을 때
			EnhancedInput->BindAction(Action.InputAction, ETriggerEvent::Started, this,
				&AWinter2025Player::Input_AbilityTagPressed, Action.InputTag);

			// 뗐을 때
			EnhancedInput->BindAction(Action.InputAction, ETriggerEvent::Completed, this,
				&AWinter2025Player::Input_AbilityTagReleased, Action.InputTag);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Setup] Invalid InputAction or InputTag in AbilityInputActions!"));
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
		EnhancedInput->BindAction(JumpAction.Get(), ETriggerEvent::Started, this, &AWinter2025Player::Jump);
		EnhancedInput->BindAction(JumpAction.Get(), ETriggerEvent::Completed, this, &AWinter2025Player::StopJumping);
	}

	// 좌클릭 바인딩
	if (LeftClickAction.Get())
	{
		// 임시; 빌드만 되도록 해놨음
		EnhancedInput->BindAction(LeftClickAction.Get(), ETriggerEvent::Started, this, &AWinter2025Player::OnLeftClick);
	}
}

void AWinter2025Player::Input_AbilityTagPressed(FGameplayTag InputTag)
{
	if (!CachedASC)
	{
		UE_LOG(LogTemp, Error, TEXT("[Input] CachedASC is NULL!"));
		return;
	}

	// 태그에 해당하는 Spec을 찾아서 InputID를 통해 입력 이벤트로 활성화
	TArray<FGameplayAbilitySpec>& AllSpecs = CachedASC->GetActivatableAbilities();

	for (FGameplayAbilitySpec& Spec : AllSpecs)
	{
		if (Spec.DynamicAbilityTags.HasTag(InputTag))
		{
			// 찾았다면 InputID를 통해 입력 이벤트로 활성화
			if (Spec.InputID >= 0)
			{
				CachedASC->AbilityLocalInputPressed(Spec.InputID);
				return;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[Input] Spec has invalid InputID: %d"), Spec.InputID);
			}
		}
	}

	UE_LOG(LogTemp, Error, TEXT("[Input] No ability found with tag: %s"), *InputTag.ToString());
}

void AWinter2025Player::Input_AbilityTagReleased(FGameplayTag InputTag)
{
	if (!CachedASC) return;

	TArray<FGameplayAbilitySpec>& AllSpecs = CachedASC->GetActivatableAbilities();
	
	for (FGameplayAbilitySpec& Spec : AllSpecs)
	{
		if (Spec.DynamicAbilityTags.HasTag(InputTag) && Spec.InputID >= 0)
		{
			CachedASC->AbilityLocalInputReleased(Spec.InputID);
			return;
		}
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

void AWinter2025Player::OnLeftClick(const FInputActionValue& Value)
{
	if (!CachedASC) {
		UE_LOG(LogTemp, Warning, TEXT("AWinter2025Player: OnLeftClick - CachedASC is null."));
		return;
	}

	// 이벤트 데이터 생성 (누가 보냈는지, 타겟은 누구인지 등)
	FGameplayEventData EventData;
	EventData.Instigator = this;
	EventData.Target = this;

	// "Input.Action.Confirm" 태그와 함께 이벤트 발행
	// 이 태그는 GA_Construction에서 기다리고 있는 태그와 정확히 일치해야 합니다.
	FGameplayTag ConfirmTag = TAG_Input_LeftClick;

	// ASC를 통해 이벤트 전송
	// 활성화된 모든 어빌리티 중, 이 태그를 기다리는(WaitGameplayEvent) 어빌리티에게 신호가 갑니다.
	CachedASC->HandleGameplayEvent(ConfirmTag, &EventData);
}
