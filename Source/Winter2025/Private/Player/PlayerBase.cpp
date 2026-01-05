#include "Player/PlayerBase.h" 
#include "AbilitySystemComponent.h" // ASC 기능을 사용하기 위해 포함
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "Player/MyPlayerState.h" //PlayerState 헤더 포함

APlayerBase::APlayerBase()
{
	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;

	// Create the camera boom component
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));

	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false;

	// Create the camera component
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));

	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}


//void APlayerBase::BeginPlay()
//{
//	Super::BeginPlay();
//
//}

void APlayerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// stub
}


// ------------------------------------------------------------------------
// [서버용 초기화]
// ------------------------------------------------------------------------
void APlayerBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버에서는 빙의가 일어난 즉시 GAS 정보를 초기화합니다.
	InitAbilityActorInfo();
}

// ------------------------------------------------------------------------
// [클라이언트용 초기화]
// ------------------------------------------------------------------------
void APlayerBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트에서는 PlayerState가 복제되어 도착하는 시점에 초기화합니다.
	// (BeginPlay 시점에는 PlayerState가 아직 NULL일 수 있기 때문입니다.)
	InitAbilityActorInfo();
}

// ------------------------------------------------------------------------
// [GAS 연결 핵심 로직]
// ------------------------------------------------------------------------
void APlayerBase::InitAbilityActorInfo()
{
	// 내 PlayerState를 가져옵니다. (MyPlayerState로 캐스팅)
	AMyPlayerState* PS = GetPlayerState<AMyPlayerState>();
	if (PS)
	{
		// GAS 시스템 초기화 (가장 중요한 부분!)
		// - OwnerActor (데이터 주인): PlayerState (데이터가 유지됨)
		// - AvatarActor (신체/표현): This (PlayerBase, 화면에 보이는 캐릭터)
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);

		// 디버깅용 로그 (잘 연결됐는지 확인)
		UE_LOG(LogTemp, Warning, TEXT("GAS Initialized for %s (Owner: PlayerState)"), *GetName());
	}
}

// ------------------------------------------------------------------------
// [ASC 가져오기]
// ------------------------------------------------------------------------
UAbilitySystemComponent* APlayerBase::GetAbilitySystemComponent() const
{
	// 내 몸(PlayerBase)에는 ASC 변수가 없습니다.
	// 대신 PlayerState에 있는 것을 빌려서 반환합니다.
	if (AMyPlayerState* PS = GetPlayerState<AMyPlayerState>())
	{
		return PS->GetAbilitySystemComponent();
	}
	return nullptr;
}