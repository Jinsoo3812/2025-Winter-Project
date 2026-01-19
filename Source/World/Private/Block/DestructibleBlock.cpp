// Fill out your copyright notice in the Description page of Project Settings.

#include "Block/DestructibleBlock.h"
#include "AbilitySystemComponent.h"
#include "Block/BlockAttributeSet.h"

ADestructibleBlock::ADestructibleBlock()
{
	// 블록이 파괴 가능하도록 설정
	IsDestrictible = true;

	// 블록이 추락할 수 있도록 설정
	bCanFall = true;

	// Ability System Component 생성
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// [네트워크] ASC 복제 설정 (멀티플레이 필수)
	AbilitySystemComponent->SetIsReplicated(true);

	// [네트워크] 복제 모드 설정
	// Minimal: GameplayEffect를 클라이언트에 복제하지 않음 (미니언, AI, 파괴물 등에 적합)
	// Mixed: PlayerState를 사용하는 플레이어에게 적합
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// Attribute Set 생성
	BlockAttributeSet = CreateDefaultSubobject<UBlockAttributeSet>(TEXT("BlockAttributeSet"));
}

void ADestructibleBlock::BeginPlay()
{
	Super::BeginPlay();
	// ASC가 유효한 경우 Gameplay Effect 적용 콜백 등록
	if (AbilitySystemComponent)
	{	
		// ASC 초기화
		// Block은 플레이어와 달리 매번 새로운 블록을 생성하므로 자신을 소유자로 설정
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("DestructibleBlock: AbilitySystemComponent is null in DestructibleBlock %s"), *GetName());
	}

	if (BlockAttributeSet)
	{
		// 체력이 0이 되면 HandleOutOfHealth 함수가 호출되도록 등록
		BlockAttributeSet->OnDestroyed.AddUObject(this, &ADestructibleBlock::HandleDestroyed);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("DestructibleBlock: BlockAttributeSet is null in %s"), *GetName());
	}
}

UAbilitySystemComponent* ADestructibleBlock::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADestructibleBlock::HandleDestroyed(AActor * InstigatorActor)
{
	SelfDestroy();
}

void ADestructibleBlock::SelfDestroy()
{
	// 위 블록에게 낙하하라고 알림
	NotifyUpperBlock();
	Destroy();
}
