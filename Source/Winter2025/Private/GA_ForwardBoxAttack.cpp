// Fill out your copyright notice in the Description page of Project Settings.

#include "GA_ForwardBoxAttack.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

UGA_ForwardBoxAttack::UGA_ForwardBoxAttack()
{
	// 객체 생성 정책: GA를 소유한 액터마다 '하나의 GA 객체만' 생성
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 서버-클라이언트 통신 정책: 클라이언트의 입력이 발생하면, 서버의 응답을 기다리지 않고 즉시(예측) 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_ForwardBoxAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// CommitAbility: 자원 소모(마나, 스태미너 등) 및 쿨다운을 검사하고, 모두 충족되면 자원을 소모하고 쿨다운을 시작
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// 자원 소모 또는 쿨다운 검사 실패 시 GA 종료
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// GA 소유 액터를 가져오고 유효성 검사
	AActor* OwnerActor = ActorInfo->OwnerActor.Get();
	if (!OwnerActor)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 디버그를 위한 박스 충돌 영역 계산 및 시각화
	FVector OwnerLocation = OwnerActor->GetActorLocation();
	FVector ForwardVector = OwnerActor->GetActorForwardVector();
	FVector BoxCenter = OwnerLocation + ForwardVector * ForwardOffset;

	FVector HalfExtent = FVector(BoxDepth * 0.5f, BoxWidth * 0.5f, BoxHeight * 0.5f);
	FQuat BoxRotation = OwnerActor->GetActorQuat();

	TArray<FOverlapResult> OverlapResults;
	FCollisionShape BoxShape = FCollisionShape::MakeBox(HalfExtent);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerActor);

	bool bHit = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		BoxCenter,
		BoxRotation,
		ECC_Pawn,
		BoxShape,
		QueryParams
	);

	DrawDebugBox(GetWorld(), BoxCenter, HalfExtent, BoxRotation, FColor::Red, false, DebugDrawDuration, 0, 3.0f);

	// 충돌하였고, TestGameplayEffect(GE)가 유효할 경우
	if (bHit && TestGameplayEffect)
	{
		for (const FOverlapResult& Result : OverlapResults)
		{
			if (AActor* HitActor = Result.GetActor())
			{
				UAbilitySystemComponent* TargetASC = nullptr;
				
				if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(HitActor))
				{
					TargetASC = ASI->GetAbilitySystemComponent();
				}
				
				if (TargetASC)
				{
					// GE를 만들기 위한 정보를 담은 EffectContext 생성 및 출처(OwnerActor) 설정
					FGameplayEffectContextHandle EffectContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
					EffectContext.AddSourceObject(OwnerActor);

					// 
					FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(TestGameplayEffect, GetAbilityLevel(), EffectContext);
					if (SpecHandle.IsValid())
					{
						// GE Spec에 동적으로 태그 추가 (Block.Destruction)
						SpecHandle.Data->DynamicGrantedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Block.Category.Destruction")));

						GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

						UE_LOG(LogTemp, Warning, TEXT("Applied GE to: %s"), *HitActor->GetName());
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("Failed to create GameplayEffectSpec for GE in GA_ForwardBoxAttack by %s"), *GetName());
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Hit actor %s does not have an AbilitySystemComponent."), *HitActor->GetName());
				}
			}
		}
	}
	else if (!TestGameplayEffect)
	{
		UE_LOG(LogTemp, Error, TEXT("TestGameplayEffect is null in GA_ForwardBoxAttack by %s"), *GetName());
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
