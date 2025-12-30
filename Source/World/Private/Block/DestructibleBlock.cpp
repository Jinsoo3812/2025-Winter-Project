// Fill out your copyright notice in the Description page of Project Settings.

#include "Block/DestructibleBlock.h"
#include "AbilitySystemComponent.h"

ADestructibleBlock::ADestructibleBlock()
{
	// 블록이 파괴 가능하도록 설정
	IsDestrictible = true;

	// Ability System Component 생성
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
}

void ADestructibleBlock::BeginPlay()
{
	Super::BeginPlay();
	// ASC가 유효한 경우 Gameplay Effect 적용 콜백 등록
	if (AbilitySystemComponent)
	{	
		// Gameplay Effect가 적용될 때 호출되는 델리게이트 바인딩
		AbilitySystemComponent->OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &ADestructibleBlock::OnGameplayEffectApplied);

		// ASC 초기화
		// void UAbilitySystemComponent::InitAbilityActorInfo(AActor* OwnerActor, AActor* AvatarActor);
		// 일반적으로 Owner와 Avatar는 동일한 액터(자신)으로 설정한다.
		// 멀티플레이 게임에서는 Owner와 Avatar가 다를 수 있다.
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("AbilitySystemComponent is null in DestructibleBlock %s"), *GetName());
	}
}

UAbilitySystemComponent* ADestructibleBlock::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADestructibleBlock::OnGameplayEffectApplied(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	UE_LOG(LogTemp, Warning, TEXT("DestructibleBlock %s received a Gameplay Effect."), *GetName());
	// DestructionTag이 유효하지 않으면 리턴
	if (!DestructionTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("DestructionTag is not valid in DestructibleBlock %s"), *GetName());
		return;
	}

	// GE를 만든 주체의 Tag. 주체가 GE에 파괴 관련 Tag를 부여하도록 구현할 것이므로 비교할 필요 X
	// const FGameplayTagContainer* SourceTags = SpecApplied.CapturedSourceTags.GetAggregatedTags();
	
	// GE 자체의 Tag (GE를 만든 주체가 선택적으로 Tag를 GE에 부여)
	const FGameplayTagContainer& SpecTags = SpecApplied.Def->InheritableGameplayEffectTags.CombinedTags;

	// 태그 비교: SpecTags에 DestructionTag가 포함되어 있는지 확인
	bool bHasDestructionTag = false;
	UE_LOG(LogTemp, Warning, TEXT("DestructibleBlock %s received GE with Tags: %s"), *GetName(), *SpecTags.ToString());
	if (SpecTags.HasTag(DestructionTag))
	{
		bHasDestructionTag = true;
	}

	// DestructionTag가 있으면 자신을 파괴
	if (bHasDestructionTag)
	{
		SelfDestroy();
	}
}

void ADestructibleBlock::SelfDestroy()
{
	// 레벨에서 자신을 없애는 함수. AActor의 Destroy()를 호출.
	// 필요에 따라 파괴 효과나 점수 증가 등의 추가 로직을 여기에 구현할 수 있습니다.
	// 자신의 이름과 함께 로그 출력
	UE_LOG(LogTemp, Warning, TEXT("Destructible Block %s is destroyed."), *GetName());
	Destroy();
}