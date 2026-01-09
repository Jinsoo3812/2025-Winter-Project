// Fill out your copyright notice in the Description page of Project Settings.

#include "TestCharacter.h"
#include "GameplayAbilitySpec.h"
#include "Abilities/GameplayAbility.h"
#include "SkillManagerComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Player/Test/TestPlayerState.h"

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

	// 이 TestCharacter가 이 클라이언트가 조종해야 하는 로컬 캐릭터인지 확인
	if (!IsLocallyControlled())
	{
		return;
	}

	// Enhanced Input Component로 캐스팅
	// Actor 수준에서 작동하며, IA와 C++/BP 함수를 바인딩하기 위해 사용
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("ATestCharacter: PlayerInputComponent is not UEnhancedInputComponent"));
		return;
	}

	// PlayerController 가져오기
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("ATestCharacter: PlayerController is null"));
		return;
	}

	// Enhanced Input Subsystem 가져오기
	// LocalPlayer 단위로 동작하는 서브시스템
	// IMC의 추가, 제거 및 물리 키(WASD 등)와 IA의 매핑 관리
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("ATestCharacter: Enhanced Input Subsystem is null"));
		return;
	}

	// Input Mapping Context 추가
	if (InputMappingContext)
	{
		Subsystem->AddMappingContext(InputMappingContext, 0);
		UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Input Mapping Context added"));
	}

	// 스킬 슬롯별 입력 바인딩 (배열 기반)
	for (int32 i = 0; i < SkillSlotActions.Num(); ++i)
	{
		if (SkillSlotActions[i])
		{
			// Pressed 이벤트
			// @param SkillSlotActions[i]: 바인딩할 Input Action
			// @param ETriggerEvent::Started: 액션이 시작될 때 트리거
			// @param this: 바인딩할 대상 객체(호출할 함수가 있는 객체)
			// @param OnAbilityInputPressed: 호출할 멤버 함수 포인터
			// @param i: 추가 전달할 정수 매개변수 (InputID)
			EnhancedInputComponent->BindAction(
				SkillSlotActions[i], 
				ETriggerEvent::Started, 
				this, 
				&ATestCharacter::OnAbilityInputPressed, 
				i
			);
			
			// Released 이벤트 
			EnhancedInputComponent->BindAction(
				SkillSlotActions[i], 
				ETriggerEvent::Completed, 
				this, 
				&ATestCharacter::OnAbilityInputReleased, 
				i
			);
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

void ATestCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버에서만 호출됨
	UE_LOG(LogTemp, Log, TEXT("ATestCharacter::PossessedBy - Server initialization"));

	// PlayerState에서 ASC와 SkillManager 가져오기
	ATestPlayerState* PS = GetPlayerState<ATestPlayerState>();
	if (PS)
	{
		// ASC와 SkillManager 캐싱
		CachedAbilitySystemComponent = PS->GetAbilitySystemComponent();
		CachedSkillManager = PS->GetSkillManager();

		if (CachedAbilitySystemComponent)
		{
			// InitAbilityActorInfo 호출
			// OwnerActor: PlayerState (ASC를 소유)
			// AvatarActor: TestCharacter (레벨에서 실제로 행동하는 액터)
			CachedAbilitySystemComponent->InitAbilityActorInfo(PS, this);

			// 공통 초기화 함수 호출
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

	// 클라이언트에서만 호출됨
	UE_LOG(LogTemp, Log, TEXT("ATestCharacter::OnRep_PlayerState - Client initialization"));

	// PlayerState에서 ASC와 SkillManager 가져오기
	ATestPlayerState* PS = GetPlayerState<ATestPlayerState>();
	if (PS)
	{
		// ASC와 SkillManager 캐싱
		CachedAbilitySystemComponent = PS->GetAbilitySystemComponent();
		CachedSkillManager = PS->GetSkillManager();

		if (CachedAbilitySystemComponent)
		{
			// InitAbilityActorInfo 호출
			CachedAbilitySystemComponent->InitAbilityActorInfo(PS, this);

			// 공통 초기화 함수 호출
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

