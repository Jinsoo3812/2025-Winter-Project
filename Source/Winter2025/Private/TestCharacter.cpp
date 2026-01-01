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
	// Actor 수준에서 작동
	// IA와 C++/BP 함수를 바인딩
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

	// 스킬 슬롯별 입력 바인딩
	if (SkillSlot0Action)
	{
		EnhancedInputComponent->BindAction(SkillSlot0Action, ETriggerEvent::Started, this, &ATestCharacter::OnSkillSlot0Triggered);
	}

	if (SkillSlot1Action)
	{
		EnhancedInputComponent->BindAction(SkillSlot1Action, ETriggerEvent::Started, this, &ATestCharacter::OnSkillSlot1Triggered);
	}

	if (SkillSlot2Action)
	{
		EnhancedInputComponent->BindAction(SkillSlot2Action, ETriggerEvent::Started, this, &ATestCharacter::OnSkillSlot2Triggered);
	}

	if (SkillSlot3Action)
	{
		EnhancedInputComponent->BindAction(SkillSlot3Action, ETriggerEvent::Started, this, &ATestCharacter::OnSkillSlot3Triggered);
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
		UE_LOG(LogTemp, Error, TEXT("ATestCharacter: InitializeAbilitySystem - SkillManager is null"));
		return;
	}

	// SkillManager가 유효한지 확인
	if (!CachedSkillManager)
	{
		UE_LOG(LogTemp, Error, TEXT("ATestCharacter: InitializeAbilitySystem - AbilitySystemComponent is null"));
		return;
	}

	// SkillManager 초기화
	CachedSkillManager->SkillManagerInitialize(CachedAbilitySystemComponent);

	// PlayerState에서 DefaultSkills를 가져와서 장착
	// 서버가 스폰한 TestCharacter만이 권한을 가진 진짜
	// 각 클라이언트(유저들)이 스폰한 TestCharacter는 복제본이므로 권한 없음
	if (HasAuthority())
	{
		ATestPlayerState* PS = GetPlayerState<ATestPlayerState>();
		if (PS)
		{
			const TArray<TSubclassOf<UGameplayAbility>>& DefaultSkills = PS->GetDefaultSkills();
			for (int32 i = 0; i < DefaultSkills.Num(); ++i)
			{
				if (DefaultSkills[i])
				{
					CachedSkillManager->EquipSkill(i, DefaultSkills[i]);
					UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Equipped skill at slot %d"), i);
				}
			}
		}
	}

	// 이중 초기화 방지 플래그 설정
	bAbilitySystemInitialized = true;
	UE_LOG(LogTemp, Log, TEXT("ATestCharacter: AbilitySystem initialization complete"));
}

void ATestCharacter::OnSkillSlot0Triggered()
{
	if (CachedSkillManager && bAbilitySystemInitialized)
	{
		CachedSkillManager->ActivateSkillBySlot(0);
		UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Skill Slot 0 activated"));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("ATestCharacter: Skill Slot 0 activation failed - SkillManager not initialized"));
	}
}

void ATestCharacter::OnSkillSlot1Triggered()
{
	if (CachedSkillManager && bAbilitySystemInitialized)
	{
		CachedSkillManager->ActivateSkillBySlot(1);
		UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Skill Slot 1 activated"));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("ATestCharacter: Skill Slot 1 activation failed - SkillManager not initialized"));
	}
}

void ATestCharacter::OnSkillSlot2Triggered()
{
	if (CachedSkillManager && bAbilitySystemInitialized)
	{
		CachedSkillManager->ActivateSkillBySlot(2);
		UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Skill Slot 2 activated"));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("ATestCharacter: Skill Slot 2 activation failed - SkillManager not initialized"));
	}
}

void ATestCharacter::OnSkillSlot3Triggered()
{
	if (CachedSkillManager && bAbilitySystemInitialized)
	{
		CachedSkillManager->ActivateSkillBySlot(3);
		UE_LOG(LogTemp, Log, TEXT("ATestCharacter: Skill Slot 3 activated"));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("ATestCharacter: Skill Slot 3 activation failed - SkillManager not initialized"));
	}
}

