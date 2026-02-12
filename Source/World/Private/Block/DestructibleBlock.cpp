// Fill out your copyright notice in the Description page of Project Settings.

#include "Block/DestructibleBlock.h"
#include "AbilitySystemComponent.h"
#include "Block/BlockAttributeSet.h"

void ADestructibleBlock::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{	
		// Block은 플레이어와 달리 자신을 소유자로 설정
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("DestructibleBlock: AbilitySystemComponent is null in %s"), *GetName());
	}

	if (AttributeSet)
	{
		// 체력이 0이 되면 OnDestroyed 함수가 호출되도록 등록
		AttributeSet->OnDestroyed.AddUObject(this, &ADestructibleBlock::OnDestroyed);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("DestructibleBlock: BlockAttributeSet is null in %s"), *GetName());
	}
}

void ADestructibleBlock::LifeSpanExpired()
{
	SelfDestroy();
}

void ADestructibleBlock::OnDestroyed(AActor * InstigatorActor)
{
	SelfDestroy();
}

void ADestructibleBlock::SelfDestroy()
{
	// 위 블록에게 낙하하라고 알림
	NotifyUpperBlock();
	Super::SelfDestroy();
}
