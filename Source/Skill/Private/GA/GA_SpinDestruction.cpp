// Fill out your copyright notice in the Description page of Project Settings.

#include "GA/GA_SpinDestruction.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"

UGA_SpinDestruction::UGA_SpinDestruction()
{
	// 기본 스킬 스펙 설정 (회전 스킬은 별도의 밸런스를 가짐)
	BaseDamage = 5.0f;      // 0.5초마다 적용되므로 단발 스킬보다 낮게 설정
	BaseCooldown = 8.0f;    // 회전 스킬의 쿨다운
	BaseRange = 1.0f;       // 범위 배율 기준값
	
	UE_LOG(LogTemp, Warning, TEXT("[GA_SpinDestruction] Constructor called - This is the SPIN version!"));
}

void UGA_SpinDestruction::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogTemp, Error, TEXT("=== [GA_SpinDestruction] ActivateAbility called - SPIN SKILL ACTIVATED! ==="));
	
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 회전 스킬 시작
	StartSpin();
}

void UGA_SpinDestruction::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 모든 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpinDurationTimerHandle);
		World->GetTimerManager().ClearTimer(DamageTickTimerHandle);
		World->GetTimerManager().ClearTimer(DebugDrawTimerHandle);
	}

	// 타이머 핸들 무효화
	SpinDurationTimerHandle.Invalidate();
	DamageTickTimerHandle.Invalidate();
	DebugDrawTimerHandle.Invalidate();

	// 태스크 정리
	if (WaitInputTask)
	{
		WaitInputTask->EndTask();
		WaitInputTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_SpinDestruction::StartSpin()
{
	UE_LOG(LogTemp, Error, TEXT("=== [GA_SpinDestruction] StartSpin - Beginning rotation skill! ==="));
	
	// 스킬 시전 시작 알림 (State.Busy 태그 추가)
	NotifySkillCastStarted();

	// 코스트 커밋 (쿨다운은 스킬 종료 시 적용)
	if (!CommitAbilityCost(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		UE_LOG(LogTemp, Error, TEXT("GA_SpinDestruction: Failed to commit ability cost"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_SpinDestruction: World is null"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	// 스킬 키 재입력 감지 태스크 생성
	WaitInputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (WaitInputTask)
	{
		// OnPress 델리게이트에 콜백 함수(조기 종료) 바인딩
		WaitInputTask->OnPress.AddDynamic(this, &UGA_SpinDestruction::OnSkillKeyPressed);

		// 어빌리티 태스크 활성화
		WaitInputTask->ReadyForActivation();
	}

	UE_LOG(LogTemp, Warning, TEXT("[GA_SpinDestruction] Setting up timers: Duration=%.1fs, TickInterval=%.2fs, Radius=%.1f"), 
		SpinDuration, DamageTickInterval, SpinRadius);

	// 데미지 적용 주기적 호출 (0.5초마다)
	World->GetTimerManager().SetTimer(
		DamageTickTimerHandle,
		this,
		&UGA_SpinDestruction::ApplySpinDamage,
		DamageTickInterval,
		true,
		0.0f // 즉시 시작
	);

	// 디버그 드로우 업데이트 (매 프레임)
	World->GetTimerManager().SetTimer(
		DebugDrawTimerHandle,
		this,
		&UGA_SpinDestruction::UpdateDebugDraw,
		0.016f,
		true
	);

	// 지속 시간 후 자동 종료
	World->GetTimerManager().SetTimer(
		SpinDurationTimerHandle,
		this,
		&UGA_SpinDestruction::StopSpin,
		SpinDuration,
		false
	);

	UE_LOG(LogTemp, Error, TEXT("=== [GA_SpinDestruction] Spin started successfully! You should see a green sphere! ==="));
}

void UGA_SpinDestruction::StopSpin()
{
	UE_LOG(LogTemp, Log, TEXT("GA_SpinDestruction: Spin stopped"));

	// 회전 스킬 종료 시 쿨다운 적용
	ApplyCooldown(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());

	// 스킬 종료
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_SpinDestruction::ApplySpinDamage()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_SpinDestruction: AvatarActor is null"));
		return;
	}

	// 플레이어 위치 기준으로 원형 범위 계산
	FVector OwnerLocation = AvatarActor->GetActorLocation();

	// 룬 적용된 반지름 계산
	float RangeMultiplier = GetRuneModifiedRange() / BaseRange;
	float AdjustedRadius = SpinRadius * RangeMultiplier;

	// 충돌 검사 파라미터
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FOverlapResult> OverlapResults;

	// 구형 범위로 충돌 검사
	bool bHit = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		OwnerLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(AdjustedRadius),
		QueryParams
	);

	if (bHit)
	{
		for (const FOverlapResult& Overlap : OverlapResults)
		{
			AActor* HitActor = Overlap.GetActor();
			if (!HitActor) continue;

			// ASC 확인
			UAbilitySystemComponent* TargetASC = nullptr;
			if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(HitActor))
			{
				TargetASC = ASI->GetAbilitySystemComponent();
			}

			if (TargetASC)
			{
				// 데미지 Effect 적용
				FGameplayEffectSpecHandle DamageSpecHandle = MakeRuneDamageEffectSpec(CurrentSpecHandle, CurrentActorInfo);
				if (DamageSpecHandle.IsValid())
				{
					GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
						*DamageSpecHandle.Data.Get(),
						TargetASC
					);
				}

				// 파괴 Effect 적용
				if (DestructionEffect)
				{
					FGameplayEffectContextHandle DestructionContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
					DestructionContext.AddSourceObject(AvatarActor);

					FGameplayEffectSpecHandle DestructionSpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(
						DestructionEffect,
						GetAbilityLevel(),
						DestructionContext
					);

					if (DestructionSpecHandle.IsValid())
					{
						GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
							*DestructionSpecHandle.Data.Get(),
							TargetASC
						);
					}
				}
			}
		}
	}
}

void UGA_SpinDestruction::UpdateDebugDraw()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !GetWorld())
	{
		return;
	}

	// 플레이어 위치 기준
	FVector OwnerLocation = AvatarActor->GetActorLocation();

	// 룬 적용된 반지름 계산
	float RangeMultiplier = GetRuneModifiedRange() / BaseRange;
	float AdjustedRadius = SpinRadius * RangeMultiplier;

	// 디버그 원형 그리기 (초록색)
	DrawDebugSphere(
		GetWorld(),
		OwnerLocation,
		AdjustedRadius,
		32, // 세그먼트 수
		FColor::Green,
		false,
		0.05f, // 0.05초 동안 표시 (다음 프레임까지)
		0,
		2.0f // 선 두께
	);
}

void UGA_SpinDestruction::OnSkillKeyPressed(float TimeWaited)
{
	// 스킬 키 재입력 시 조기 종료
	StopSpin();
}

