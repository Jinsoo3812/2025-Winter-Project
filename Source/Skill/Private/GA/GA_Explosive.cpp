// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/GA_Explosive.h"
#include "Object/Explosive.h"
#include "Block/BlockBase.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

UGA_Explosive::UGA_Explosive()
{
}

void UGA_Explosive::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// GA_StickyBomb과 달리 기존 폭탄을 확인하거나 기폭하는 로직이 없음.
	// 즉시 투척을 위한 조준(Preview) 모드로 진입.

	// 프리뷰 타이머 시작
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TickTimerHandle, this, &UGA_Explosive::UpdatePreview, 0.016f, true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: World is null, cannot start Preview Timer"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 취소 입력 대기 (스킬 재입력시 조준 취소)
	InputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (InputTask)
	{
		InputTask->OnPress.AddDynamic(this, &UGA_Explosive::OnCancelPressed);
		InputTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: Failed to create WaitInputTask for targeting"));
	}

	// 좌클릭 바인딩 (투척 확정)
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			PC->InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &UGA_Explosive::OnLeftClickPressed);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("GA_Explosive: PlayerController or InputComponent is null"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: OwnerPawn is null"));
	}
}

void UGA_Explosive::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}
	TickTimerHandle.Invalidate();

	// 하이라이트 정리
	ClearHighlights();

	// 태스크 정리
	if (InputTask)
	{
		InputTask->EndTask();
		InputTask = nullptr;
	}

	// 입력 바인딩 해제
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			for (int32 i = PC->InputComponent->KeyBindings.Num() - 1; i >= 0; --i)
			{
				if (PC->InputComponent->KeyBindings[i].KeyDelegate.GetUObject() == this)
				{
					PC->InputComponent->KeyBindings.RemoveAt(i);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GA_Explosive: PC or InputComponent null during EndAbility cleanup"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_Explosive: OwnerPawn null during EndAbility cleanup"));
	}

	// 저장된 타겟 초기화
	SavedTargetBlock.Reset();

	NotifySkillCastFinished();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Explosive::UpdatePreview()
{
	// GA_StickyBomb과 동일한 로직 사용
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn) return;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) return;

	// 1. 이전 프레임의 하이라이트 초기화
	ClearHighlights();

	// 2. 사거리 내 블록 탐색
	TArray<ABlockBase*> BlocksInRange;
	TArray<AActor*> ActorsInRange;
	FindBlocksInRange(ActorsInRange);
	
	// AActor*를 ABlockBase*로 변환
	for (AActor* Actor : ActorsInRange)
	{
		if (ABlockBase* Block = Cast<ABlockBase>(Actor))
		{
			BlocksInRange.Add(Block);
		}
	}

	// 3. 탐색된 블록들에 일괄적으로 'Preview(파랑)' 상태 적용
	BatchHighlightBlocks(ActorsInRange, 1.0f);

	// 4. 나중에 끄기 위해 목록 백업
	PreviewedBlocks = BlocksInRange;

	// 5. 마우스 커서 위치의 블록 타겟팅 처리
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Visibility, true, HitResult);
	ABlockBase* HitBlock = Cast<ABlockBase>(HitResult.GetActor());

	// 마우스 밑의 블록이 사거리(파란 영역) 안에 포함되어 있다면 'Targeted(초록)'으로 덮어쓰기
	if (HitBlock && PreviewedBlocks.Contains(HitBlock))
	{
		// Targeted = 2.0f
		TArray<AActor*> TargetedActor;
		TargetedActor.Add(HitBlock);
		BatchHighlightBlocks(TargetedActor, 2.0f);
		HighlightedBlock = HitBlock;
	}
	else
	{
		HighlightedBlock.Reset();
	}
}

void UGA_Explosive::OnLeftClickPressed()
{
	// 하이라이트된 블록이 유효할 때만 시전
	if (HighlightedBlock.IsValid())
	{
		// 스킬 시전 시작 (Busy 태그 등 적용)
		NotifySkillCastStarted();

		// 폭발물 투척 및 스킬 종료 (단발성이므로 즉시 종료됨)
		SpawnExplosive();
	}
	else
	{
		// 타겟이 유효하지 않을 때는 아무 동작 하지 않음 (로그 생략 가능)
	}
}

void UGA_Explosive::OnCancelPressed(float TimeWaited)
{
	// 조준 취소 및 종료
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UGA_Explosive::SpawnExplosive()
{
	// 코스트 지불 확인
	if (!CommitAbilityCost(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_Explosive: Failed to commit ability cost"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	if (!ExplosiveClass)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: ExplosiveClass is not set"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	// 목표 블록 백업
	SavedTargetBlock = HighlightedBlock.Get();

	// 프리뷰 종료 및 입력 해제 로직 (EndAbility에서 처리하지만 명시적 정리를 위해)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}
	ClearHighlights();

	// 입력 태스크 종료
	if (InputTask)
	{
		InputTask->EndTask();
		InputTask = nullptr;
	}

	// 폭발물 생성
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		FVector SpawnLoc = OwnerPawn->GetActorLocation();
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AExplosive* NewExplosive = GetWorld()->SpawnActor<AExplosive>(ExplosiveClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);

		if (NewExplosive && SavedTargetBlock.IsValid())
		{
			// 데미지 스펙 핸들 생성
			FGameplayEffectSpecHandle DamageSpecHandle = MakeRuneDamageEffectSpec(CurrentSpecHandle, CurrentActorInfo);

			// 폭발물 초기화
			// 중요: AutoDetonateDelay를 0.0f로 설정하여 착탄(OnLanded) 후 즉시 폭발하도록 함
			NewExplosive->Initialize(
				SpawnLoc,
				SavedTargetBlock.Get(),
				1.5f,			// FlightDuration
				0.01f,			// AutoDetonateDelay (즉시 폭발)
				ExplosionRadius * GetRuneModifiedRange(),
				1,				// MaxBombCount (이 스킬에선 의미 없지만 1로 전달)
				GetAbilitySystemComponentFromActorInfo(),
				DamageSpecHandle,
				DestructionEffect
			);

			// 점착 폭탄과 달리 리스트에 추가하거나 델리게이트를 기다리지 않음.
			// 던지는 행위로 스킬 사용은 끝남.

			// 투척 즉시 쿨타임 적용
			CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);

			// 스킬 정상 종료
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("GA_Explosive: Failed to spawn bomb or invalid target"));
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: OwnerPawn is null during spawn"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UGA_Explosive::ClearHighlights()
{
	// 저장된 프리뷰 블록들의 상태를 'None'으로 복구
	TArray<AActor*> ActorsToReset;
	for (ABlockBase* Block : PreviewedBlocks)
	{
		if (Block)
		{
			ActorsToReset.Add(Block);
		}
	}
	BatchHighlightBlocks(ActorsToReset, 0.0f);

	// 목록 초기화
	PreviewedBlocks.Empty();
	HighlightedBlock.Reset();
}