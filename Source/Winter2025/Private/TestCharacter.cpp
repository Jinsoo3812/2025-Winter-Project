// Fill out your copyright notice in the Description page of Project Settings.

#include "TestCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Player/Test/TestPlayerState.h"
#include "Player/PlayerAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "InputAction.h"


ATestCharacter::ATestCharacter()
{
	// ASC와 SkillManager는 더 이상 여기서 생성하지 않음
	// PlayerState에서 관리됨
	
	// 스킬 슬롯 Input Action 배열 초기화 (최대 3개)
	SkillSlotActions.SetNum(3);
}

UAbilitySystemComponent* ATestCharacter::GetAbilitySystemComponent() const
{
	// 캐시된 ASC 반환
	return CachedAbilitySystemComponent;
}

void ATestCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 1. Enhanced Input Component로 한 번만 캐스팅해서 변수에 저장
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error, TEXT("ATestCharacter: PlayerInputComponent is not UEnhancedInputComponent"));
		return;
	}

	// 2. 로컬 플레이어인 경우에만 Input Mapping Context(IMC) 추가
	// (AI나 서버 측 캐릭터는 입력 매핑이 필요 없으므로 체크)
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

	// 3. 이동 (Move) 바인딩
	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATestCharacter::Move);
	}

	// 4. 점프 (Jump) 바인딩
	if (JumpAction)
	{
		// ACharacter::Jump는 존재하지만, StopJump는 없습니다. StopJumping을 써야 합니다.
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}

	// 5. 스킬 슬롯 입력 바인딩
	for (int32 i = 0; i < SkillSlotActions.Num(); ++i)
	{
		if (SkillSlotActions[i])
		{
			// Pressed
			EnhancedInput->BindAction(SkillSlotActions[i], ETriggerEvent::Started, this, &ATestCharacter::OnAbilityInputPressed, i);

			// Released
			EnhancedInput->BindAction(SkillSlotActions[i], ETriggerEvent::Completed, this, &ATestCharacter::OnAbilityInputReleased, i);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Input bindings set up successfully"));
}
void ATestCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	// BeginPlay에서는 초기화하지 않음
	// PlayerState는 서버에서 PossessedBy, 클라이언트에서 OnRep_PlayerState가 호출될 때까지 보장되지 않음
	// PossessedBy(서버) 또는 OnRep_PlayerState(클라이언트)에서 초기화됨

}


void ATestCharacter::Move(const FInputActionValue& Value)
{
	// 1. 입력값 가져오기 (X, Y)
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// 2. 컨트롤러(카메라)가 바라보는 방향 알아내기
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0); // Pitch, Roll 무시하고 Yaw(수평 회전)만 사용

		// 3. 전방(Forward) 및 우측(Right) 방향 벡터 계산
		// (카메라가 보는 방향 기준으로 앞/오른쪽을 구함)
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// 4. 이동 입력 적용
		AddMovementInput(ForwardDirection, MovementVector.Y); // W/S (앞뒤)
		AddMovementInput(RightDirection, MovementVector.X);   // A/D (좌우)
	}
}


void ATestCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버에서만 호출됨
	UE_LOG(LogTemp, Log, TEXT("ATestCharacter::PossessedBy - Server initialization"));

	// PlayerState에서 ASC와 SkillManager 가져오기
	ATestPlayerState* PS = GetPlayerState<ATestPlayerState>();
	if (PS)
	{
		// ASC, SkillManager, AttributeSet 캐시
		CachedAbilitySystemComponent = PS->GetAbilitySystemComponent();
		CachedSkillManager = PS->GetSkillManager();
		CachedAttributeSet = Cast<UAttributeSet>(PS->GetAttributeSet());

		if (CachedAbilitySystemComponent)
		{
			// InitAbilityActorInfo 호출
			// OwnerActor: PlayerState (ASC를 소유)
			// AvatarActor: TestCharacter (실제로 게임에서 행동하는 액터)
			CachedAbilitySystemComponent->InitAbilityActorInfo(PS, this);

			// 나머지 초기화 함수 호출
			InitializeAbilitySystem();
			
			UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Server - AbilitySystem initialized successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ATestCharacter: Server - AbilitySystemComponent is null in PlayerState"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ATestCharacter: Server - PlayerState is null"));
	}
}

void ATestCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트에서 호출됨
	UE_LOG(LogTemp, Log, TEXT("ATestCharacter::OnRep_PlayerState - Client initialization"));

	// PlayerState에서 ASC와 SkillManager 가져오기
	ATestPlayerState* PS = GetPlayerState<ATestPlayerState>();
	if (PS)
	{
		// ASC, SkillManager, AttributeSet 캐시
		CachedAbilitySystemComponent = PS->GetAbilitySystemComponent();
		CachedSkillManager = PS->GetSkillManager();
		CachedAttributeSet = Cast<UAttributeSet>(PS->GetAttributeSet());

		if (CachedAbilitySystemComponent)
		{
			// InitAbilityActorInfo 호출
			CachedAbilitySystemComponent->InitAbilityActorInfo(PS, this);

			// 나머지 초기화 함수 호출
			InitializeAbilitySystem();
			
			UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Client - AbilitySystem initialized successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ATestCharacter: Client - AbilitySystemComponent is null in PlayerState"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ATestCharacter: Client - PlayerState is null"));
	}
}

void ATestCharacter::InitializeAbilitySystem()
{
	// 이미 초기화되었다면 중복 실행 방지
	if (bAbilitySystemInitialized)
	{
		return;
	}

	// ASC가 유효한지 확인
	if (!CachedAbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("ATestCharacter: InitializeAbilitySystem - AbilitySystemComponent is null"));
		return;
	}

	// SkillManager가 유효한지 확인
	if (!CachedSkillManager)
	{
		UE_LOG(LogTemp, Error, TEXT("ATestCharacter: InitializeAbilitySystem - SkillManager is null"));
		return;
	}

	// 서버에서만 스킬 초기화 수행 (권한이 있는 진짜 캐릭터)
	if (HasAuthority())
	{
		ATestPlayerState* PS = GetPlayerState<ATestPlayerState>();
		if (PS)
		{
			// PlayerState의 InitializeSkills 호출
			// 이 함수가 초록 룬 감지 및 GA 교체를 포함한 모든 스킬 초기화 수행
			PS->InitializeSkills();
			UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Skills initialized via PlayerState"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ATestCharacter: PlayerState is null during skill initialization"));
		}
	}


	/**
	* Attribute 변화 델리게이트 바인딩
	*/
	// MovementSpeed 변경 델리게이트 바인딩
	CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UPlayerAttributeSet::GetMovementSpeedAttribute()
	).AddUObject(this, &ATestCharacter::OnMovementSpeedChanged);

	// 이중 초기화 방지 플래그 설정
	bAbilitySystemInitialized = true;
	UE_LOG(LogTemp, Log, TEXT("ATestCharacter: AbilitySystem initialization complete"));
}

void ATestCharacter::OnAbilityInputPressed(int32 InputID)
{
	if (!CachedAbilitySystemComponent || !bAbilitySystemInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("ATestCharacter: Ability input pressed but system not initialized"));
		return;
	}

	// ASC에게 입력 Press 이벤트 전달
	// AbilityLocalInputPressed가 다음 두 가지를 모두 처리:
	// 1. 실행 중인 Ability의 Task에게 신호 전파 (예: WaitInputPress)
	// 2. 비활성 상태면 자동으로 활성화 시도
	CachedAbilitySystemComponent->AbilityLocalInputPressed(InputID);
}

void ATestCharacter::OnAbilityInputReleased(int32 InputID)
{
	if (!CachedAbilitySystemComponent || !bAbilitySystemInitialized)
	{	
		UE_LOG(LogTemp, Warning, TEXT("ATestCharacter: Ability input released but system not initialized"));
		return;
	}

	// ASC에게 입력 Release 이벤트 전달
	CachedAbilitySystemComponent->AbilityLocalInputReleased(InputID);
}

void ATestCharacter::OnMovementSpeedChanged(const FOnAttributeChangeData& Data)
{
	// 실제 이동 컴포넌트에 값 적용
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = Data.NewValue;
		UE_LOG(LogTemp, Log, TEXT("ATestCharacter: MovementSpeed changed to %f"), Data.NewValue);
	}
}

