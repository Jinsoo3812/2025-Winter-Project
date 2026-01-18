// Fill out your copyright notice in the Description page of Project Settings.

#include "GA/GA_Explosive.h"
#include "Object/Explosive.h"
#include "BlockInfoInterface.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "BlockGameplayTags.h"
#include "InputGameplayTags.h"
#include "Collision/CollisionChannels.h"

UGA_Explosive::UGA_Explosive()
{
	// 인스턴싱 정책 (StickyBomb과 동일하게 유지)
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_Explosive::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

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

	// 취소 입력 대기
	InputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (InputTask)
	{
		InputTask->OnPress.AddDynamic(this, &UGA_Explosive::OnCancelPressed);
		InputTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: Failed to create WaitInputTask"));
	}

	// 좌클릭 이벤트 대기 태스크 생성
	UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		TAG_Input_LeftClick,
		nullptr,
		false,
		false
	);

	if (WaitEventTask)
	{
		WaitEventTask->EventReceived.AddDynamic(this, &UGA_Explosive::OnLeftClickEventReceived);
		WaitEventTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: Failed to create WaitGameplayEvent task"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
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
	ClearHighlights(PreviewBlocks);

	// 입력 태스크 정리
	if (InputTask)
	{
		InputTask->EndTask();
		InputTask = nullptr;
	}

	SavedTargetBlock.Reset();
	NotifySkillCastFinished();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Explosive::UpdatePreview()
{
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn) return;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) return;

	// 프리뷰 하이라이트 초기화 및 재생성
	HighlightBlocks(PreviewBlocks, TAG_Block_Highlight_Preview);

	// 마우스 커서 위치의 블록 타겟팅 처리
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Block, true, HitResult);
	AActor* HitActor = HitResult.GetActor();

	// 마우스 밑의 블록이 사거리(파란 영역) 안에 포함되어 있다면 'Targeted' 태그 적용
	bool bIsInPreviewBlocks = false;
	for (const TWeakObjectPtr<AActor>& WeakBlock : PreviewBlocks)
	{
		if (WeakBlock.IsValid() && WeakBlock.Get() == HitActor)
		{
			bIsInPreviewBlocks = true;
			break;
		}
	}

	if (HitActor && bIsInPreviewBlocks)
	{
		TArray<TWeakObjectPtr<AActor>> TargetedActor = { HitActor };
		BatchHighlightBlocks(TargetedActor, TAG_Block_Highlight_Target);
		HighlightedBlock = HitActor;
	}
	else
	{
		HighlightedBlock.Reset();
	}
}

void UGA_Explosive::OnLeftClickEventReceived(FGameplayEventData Payload)
{
	if (HighlightedBlock.IsValid())
	{
		NotifySkillCastStarted();
		SpawnExplosive();
	}
	else
	{
		// 프리뷰가 유효하지 않을 때 클릭하면 로그 (디버깅용)
		UE_LOG(LogTemp, Verbose, TEXT("GA_Explosive: Clicked but invalid preview"));
	}
}

void UGA_Explosive::OnCancelPressed(float TimeWaited)
{
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UGA_Explosive::SpawnExplosive()
{
	if (!CommitAbilityCost(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_Explosive: Failed to commit cost"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	if (!ExplosiveClass)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: ExplosiveClass is null"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	// 타겟 블록 정보 백업
	SavedTargetBlock = HighlightedBlock.Get();

	// 타이머 및 프리뷰 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}
	ClearHighlights(PreviewBlocks);

	// 입력 태스크 정리
	if (InputTask)
	{
		InputTask->EndTask();
		InputTask = nullptr;
	}

	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	// 폭발물 생성
	FVector SpawnLoc = OwnerPawn ? OwnerPawn->GetActorLocation() : FVector::ZeroVector;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AExplosive* NewExplosive = GetWorld()->SpawnActor<AExplosive>(ExplosiveClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);

	if (NewExplosive && SavedTargetBlock.IsValid())
	{
		FGameplayEffectSpecHandle DamageSpecHandle = MakeRuneDamageEffectSpec(CurrentSpecHandle, CurrentActorInfo);

		// 인터페이스를 통해 위치 정보 획득 (그리드 중심 좌표)
		// SavedTargetBlock은 이제 AActor* (WeakPtr.Get())
		IBlockInfoInterface* BlockInfo = Cast<IBlockInfoInterface>(SavedTargetBlock.Get());
		// 실제로는 AExplosive 내부에서 AttachParent로 사용되므로 Actor*를 넘기지만, 
		// 만약 타겟 위치 보정이 필요하다면 BlockInfo를 활용할 수 있음.

		NewExplosive->Initialize(
			SpawnLoc,
			SavedTargetBlock.Get(),
			1.5f,
			0.01f, // StickyBomb과 다르게 즉시 폭발 (0.01f)
			ExplosionRadius * GetRuneModifiedRange(),
			1,     // MaxBombCount (1개)
			GetAbilitySystemComponentFromActorInfo(),
			DamageSpecHandle,
			DestructionEffect
		);

		// 투척 즉시 쿨타임 및 종료
		CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: Failed to spawn bomb or invalid target"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}