#include "EnemyBase.h"
#include "AbilitySystemComponent.h"
// EnemyAttributeSet.h는 헤더에 포함되어 있으므로 여기서는 생략 가능하지만, 명시적으로 적어도 무방합니다.

AEnemyBase::AEnemyBase()
{
	// ------------------------------------------------------------------------------------------
	// 1. Ability System Component (ASC) 생성
	// ------------------------------------------------------------------------------------------
	// 적 캐릭터가 생성될 때, GAS 시스템을 처리할 컴포넌트를 함께 만듭니다.
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// [네트워크 동기화 설정]
	// 멀티플레이 게임이므로, 서버에서의 상태 변화가 클라이언트(플레이어들)에게도 전달되도록 설정합니다.
	AbilitySystemComponent->SetIsReplicated(true);

	// [작동 모드 설정: Minimal]
	// - Minimal: 적(AI)은 플레이어처럼 복잡한 입력 예측이 필요 없으므로 'Minimal'로 설정하여 네트워크 대역폭을 아낍니다.
	// - Mixed: 플레이어(PlayerState 사용 시)
	// - Minimal: AI(적)
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// ------------------------------------------------------------------------------------------
	// 2. Attribute Set (속성 세트) 생성
	// ------------------------------------------------------------------------------------------
	// 적의 체력 데이터를 담을 컨테이너(AttributeSet)를 생성하여 ASC와는 별도로 메모리에 할당합니다.
	// 주의: 여기서 생성하지 않으면 포인터가 비어있어(Null), 게임 시작 시 크래시가 발생할 수 있습니다.
	Attributes = CreateDefaultSubobject<UEnemyAttributeSet>(TEXT("Attributes"));
}

// [추가] 게임 시작 시 GAS 시스템 초기화
void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	// GAS 시스템 초기화 로직
	if (AbilitySystemComponent)
	{
		// InitAbilityActorInfo(OwnerActor, AvatarActor)
		// 적(AI)은 Owner(데이터 주인)와 Avatar(신체)가 자기 자신(this)으로 동일합니다.
		// 플레이어처럼 복잡한 연결(PlayerState) 과정 없이, 바로 초기화하면 됩니다.
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// [확인용 로그] 초기화가 잘 되었는지 확인합니다.
		// UE_LOG(LogTemp, Warning, TEXT("Enemy GAS Initialized: %s"), *GetName());
	}
}

UAbilitySystemComponent* AEnemyBase::GetAbilitySystemComponent() const
{
	// 외부에서 ASC를 요청하면, 우리가 생성해둔 멤버 변수를 반환합니다.
	return AbilitySystemComponent;
}