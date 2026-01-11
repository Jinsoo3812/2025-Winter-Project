#include "Player/MyPlayerState.h"
#include "Net/UnrealNetwork.h"

AMyPlayerState::AMyPlayerState()
{
	// ------------------------------------------------------------------------
	// 1. Ability System Component (ASC) 생성
	// ------------------------------------------------------------------------
	// 캐릭터(PlayerBase)가 아니라 여기서 ASC를 생성합니다.
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// 네트워크 동기화 켜기 (서버의 데이터가 클라이언트로 전송됨)
	AbilitySystemComponent->SetIsReplicated(true);

	// [Replication Mode 변경: Mixed]
	// - PlayerState에 ASC가 있을 때는 'Mixed' 모드를 사용하는 것이 국룰입니다.
	// - 주인(Owner, 나): GameplayEffect를 예측(Prediction)하여 즉시 반응. (랙이 없어 보임)
	// - 남들(Simulated Proxy): 서버가 보내준 결과값만 받음.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// ------------------------------------------------------------------------
	// 2. Attribute Set (체력통) 생성
	// ------------------------------------------------------------------------
	AttributeSet = CreateDefaultSubobject<UPlayerAttributeSet>(TEXT("AttributeSet"));

	// ------------------------------------------------------------------------
	// 3. 네트워크 업데이트 빈도 설정
	// ------------------------------------------------------------------------
	// 기본값은 매우 낮게 설정되어 있어, 빠른 액션 게임에서는 정보 갱신이 느릴 수 있습니다.
	// 100.0f로 설정하여 초당 100회 빈도로 상태를 동기화합니다. (부드러운 멀티플레이 필수)
	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* AMyPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

