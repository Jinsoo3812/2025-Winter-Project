// Fill out your copyright notice in the Description page of Project Settings.

#include "GA/GA_Destruction.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

UGA_Destruction::UGA_Destruction() {}

void UGA_Destruction::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, 
	const FGameplayEventData* TriggerEventData)
{
	// CommitAbility: 자원 소모(마나, 스태미너 등) 및 쿨다운을 검사하고, 모두 충족되면 자원을 소모하고 쿨다운을 시작
	// Cost와 Cooldown GE가 설정되어 있다면 이 단계에서 처리됨
	// GA_SkillBase에서 룬이 적용된 쿨타임이 자동으로 적용됨
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// 자원 소모 또는 쿨다운 검사 실패 시 GA 즉시 종료
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 레벨에 존재하는 AvatarActor(플레이어 캐릭터) 가져오기
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Destruction: OwnerActor is null"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 룬이 적용된 범위 값 가져오기
	float ModifiedRange = GetRuneModifiedRange();
	
	// BoxExtent를 범위 배율만큼 조정
	// BaseRange는 BoxExtent의 기준 크기를 의미하므로, 비율을 곱함
	FVector AdjustedBoxExtent = BoxExtent * (ModifiedRange / BaseRange);

	// 직육면체 범위의 중심 위치 계산
	// 플레이어의 위치에서 전방 방향으로 (BoxDistance + AdjustedBoxExtent.X) 만큼 이동한 지점이 박스의 중심
	FVector OwnerLocation = AvatarActor->GetActorLocation();
	FVector ForwardVector = AvatarActor->GetActorForwardVector();
	FVector BoxCenter = OwnerLocation + ForwardVector * (BoxDistance + AdjustedBoxExtent.X);

	// 충돌 검사를 위한 파라미터 설정
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(AvatarActor); // 자기 자신은 검사에서 제외

	// 겹친(Overlap) 액터들을 저장할 배열
	TArray<FOverlapResult> OverlapResults;

	// 직육면체 범위 내의 모든 액터를 검사
	// OverlapMultiByChannel: 특정 채널(ECC_Pawn)에서 Box와 겹치는 모든 액터를 찾음
	bool bHit = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		BoxCenter,
		AvatarActor->GetActorQuat(), // 플레이어의 회전값을 사용하여 박스도 같은 방향으로 회전
		ECC_Pawn, // Pawn 채널에 대해 Overlap/Block 한다면 검사
		FCollisionShape::MakeBox(AdjustedBoxExtent), // 룬이 적용된 직육면체 모양 생성
		QueryParams
	);

	// 디버그용 박스 시각화 - 설정된 시간 동안 초록색 박스로 표시
	DrawDebugBox(
		GetWorld(),
		BoxCenter,
		AdjustedBoxExtent,
		AvatarActor->GetActorQuat(),
		FColor::Green,
		false, // 영구적으로 표시하지 않음
		DebugDrawDuration, // 설정된 시간 동안 표시
		0,
		5.0f // 박스 선의 두께
	);

	// 충돌한 액터가 있는 경우 처리
	if (bHit)
	{
		// 겹친 모든 액터에 대해 반복 처리
		for (const FOverlapResult& Overlap : OverlapResults)
		{
			AActor* HitActor = Overlap.GetActor();
			if (!HitActor)
			{
				continue; // 유효하지 않은 액터는 건너뜀
			}

			// 타겟이 AbilitySystemComponent를 가지고 있는지 확인
			// IAbilitySystemInterface를 구현한 액터만 GE를 받을 수 있음
			UAbilitySystemComponent* TargetASC = nullptr;
			if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(HitActor))
			{
				TargetASC = ASI->GetAbilitySystemComponent();
			}

			// 타겟에게 ASC가 있으면 GE 적용
			if (TargetASC)
			{
				// 데미지 Effect 적용 (GA_SkillBase의 DamageEffect, 룬 적용됨)
				FGameplayEffectSpecHandle DamageSpecHandle = MakeRuneDamageEffectSpec(Handle, ActorInfo);
				if (DamageSpecHandle.IsValid())
				{
					GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
						*DamageSpecHandle.Data.Get(),
						TargetASC
					);
				}

				// 파괴 Effect 적용 (GA_Destruction의 DestructionEffect, 블록 파괴용)
				if (DestructionEffect)
				{
					// Gameplay Effect Context 생성 - 이 효과가 어디서 왔는지 등의 정보를 담음
					FGameplayEffectContextHandle DestructionContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
					DestructionContext.AddSourceObject(AvatarActor);

					// Gameplay Effect Spec 생성. Spec은 무거우므로 Handle로 관리
					// Spec: GE는 데이터 에셋, Spec은 그것을 기반한 실제 인스턴스
					// SpecHandle은 무거운 Spec을 래핑하여 관리하도록 지원
					FGameplayEffectSpecHandle DestructionSpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(
						DestructionEffect,
						GetAbilityLevel(), // GA의 현재 레벨을 GE에도 적용
						DestructionContext // GE의 출처 등을 담은 EffectContext
					);

					if (DestructionSpecHandle.IsValid())
					{
						GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
							*DestructionSpecHandle.Data.Get(),
							TargetASC
						);
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("GA_Destruction: Failed to create GameplayEffectSpec for %s"), *HitActor->GetName());
				}
			}
			else
			{
				// ASC가 없는 액터는 GE를 받을 수 없음 (예: 일반 StaticMesh)
				// UE_LOG(LogTemp, Warning, TEXT("GA_Destruction: %s does not have an AbilitySystemComponent"), *HitActor->GetName());
			}
		}
	}
	else
	{
		// 범위 내에 아무 액터도 없었음
		// UE_LOG(LogTemp, Log, TEXT("GA_Destruction: No actors found in destruction range"));
	}

	// GA 종료
	// 4번 인자 bReplicateEndAbility: true - 서버에서 클라이언트로 종료를 복제
	// 5번 인자 bWasCancelled: false - 정상 종료 (취소가 아님)
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

