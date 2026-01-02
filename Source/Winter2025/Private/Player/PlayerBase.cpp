#include "Player/PlayerBase.h" 
#include "AbilitySystemComponent.h" // ASC 기능을 사용하기 위해 포함

APlayerBase::APlayerBase()
{
	// ------------------------------------------------------------------------------------------
	// 1. Ability System Component (ASC) 생성
	// ------------------------------------------------------------------------------------------
	// 플레이어의 GAS 엔진을 생성하여 메모리에 할당합니다.
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// [네트워크 동기화 설정]
	// 서버에 있는 내 체력/상태가 클라이언트(내 화면)와 동기화되도록 설정합니다.
	AbilitySystemComponent->SetIsReplicated(true);

	// [Replication Mode 설정: Minimal]
	// - Minimal: 플레이어 캐릭터(PlayerControlled)에 최적화된 모드입니다.
	// - Mixed: AI나 적 캐릭터에 주로 쓰입니다.
	// - Full: 싱글 플레이 게임에 쓰입니다.
	// 플레이어는 입력 반응속도(예측)가 중요하므로 'Minimal'을 사용합니다.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// ------------------------------------------------------------------------------------------
	// 2. Attribute Set (속성 세트) 생성
	// ------------------------------------------------------------------------------------------
	// 체력 데이터를 담을 컨테이너를 생성합니다.
	// 이걸 생성하지 않으면 게임 시작 시 'Attributes' 포인터가 비어있어(Null) 크래시가 납니다.
	Attributes = CreateDefaultSubobject<UPlayerAttributeSet>(TEXT("Attributes"));
}


void APlayerBase::BeginPlay()
{
	Super::BeginPlay();

	// ------------------------------------------------------------------------
	// GAS 초기화
	// ------------------------------------------------------------------------
	if (AbilitySystemComponent)
	{
		// InitAbilityActorInfo(Owner, Avatar)
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

UAbilitySystemComponent* APlayerBase::GetAbilitySystemComponent() const
{
	// 인터페이스를 통해 요청이 들어오면, 우리가 만들어둔 ASC를 건네줍니다.
	return AbilitySystemComponent;
}