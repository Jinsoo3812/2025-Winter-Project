// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Test/TestPlayerState.h"
#include "AbilitySystemComponent.h"
#include "SkillManagerComponent.h"
#include "Player/PlayerAttributeSet.h"

ATestPlayerState::ATestPlayerState()
{
	// ASC 생성 및 설정
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	
	// ASC의 서버-클라이언트 상태 동기화를 활성화
	AbilitySystemComponent->SetIsReplicated(true);
	
	// Replication Mode 설정
	// Minimal: GameplayEffect를 제외한 GameplayTag와 Attribute만 동기화
	// Mixed: 소유한 클라이언트(본인)에게는 모든 GameplayEffect를 동기화, 다른 클라이언트(다른 유저)에게는 Minimal
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// 스킬 매니저 컴포넌트 생성
	SkillManager = CreateDefaultSubobject<USkillManagerComponent>(TEXT("SkillManagerComponent"));

	// AttributeSet 생성 (체력, 최대체력 등의 수치 관리)
	// PlayerState에서 관리하므로 플레이어가 죽어도 데이터가 유지됨
	AttributeSet = CreateDefaultSubobject<UPlayerAttributeSet>(TEXT("AttributeSet"));

	// 서버가 PlayerState의 상태를 클라이언트에 복제하는 주기(per second)
	// 기본값은 낮기 때문에 ASC의 변경사항이 느리게 동기화될 수 있음
	SetNetUpdateFrequency(100.0f);

	// PlayerState의 데이터가 변경되지 않을 때에는 대역폭 절약을 위해 업데이트 빈도를 낮춤
	// 아무 일도 일어나지 않을 수록 Min 값으로 점진적 감소
	SetMinNetUpdateFrequency(2.0f);
}

void ATestPlayerState::BeginPlay()
{
	Super::BeginPlay();
	// 추가적인 초기화 코드 작성 가능
}

UAbilitySystemComponent* ATestPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}