// Fill out your copyright notice in the Description page of Project Settings.

#include "GA/GA_StickyBomb.h"
#include "Object/Explosive.h"
#include "BlockInfoInterface.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "BlockGameplayTags.h"
#include "Collision/CollisionChannels.h"

UGA_StickyBomb::UGA_StickyBomb()
{
	// 폭탄 리스트 상태 유지를 위해 인스턴싱 정책 설정
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_StickyBomb::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 1. 유효하지 않은 폭탄 정리
	ExplosivesList.RemoveAll([](const TWeakObjectPtr<AExplosive>& Ptr) { return !Ptr.IsValid(); });

	// 2. 기폭 조건 확인 (최대 개수 도달 혹은 기폭 준비 상태)
	if (ExplosivesList.Num() >= MaxBombCount || bIsDetonationReady)
	{
		bool bAllLanded = true;
		for (const TWeakObjectPtr<AExplosive>& WeakExplosive : ExplosivesList)
		{
			if (WeakExplosive.IsValid() && !WeakExplosive->IsAttached())
			{
				bAllLanded = false;
				break;
			}
		}

		if (bAllLanded)
		{
			PerformDetonateAndEnd();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GA_StickyBomb: Cannot detonate yet. Some bombs are still flying."));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		}
		return;
	}

	// 3. 투척 모드 진입 (프리뷰 시작)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TickTimerHandle, this, &UGA_StickyBomb::UpdatePreview, 0.016f, true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_StickyBomb: World is null"));
	}

	// 취소 입력 대기
	InputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (InputTask)
	{
		InputTask->OnPress.AddDynamic(this, &UGA_StickyBomb::OnCancelPressed);
		InputTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_StickyBomb: Failed to create WaitInputTask"));
	}

	// 좌클릭 바인딩
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			PC->InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &UGA_StickyBomb::OnLeftClickPressed);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("GA_StickyBomb: PlayerController or InputComponent is null"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_StickyBomb: OwnerPawn is null"));
	}
}

void UGA_StickyBomb::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 타이머 및 하이라이트 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}
	TickTimerHandle.Invalidate();

	ClearHighlights(PreviewBlocks);

	// 입력 태스크 정리
	if (InputTask)
	{
		InputTask->EndTask();
		InputTask = nullptr;
	}

	// 좌클릭 바인딩 수동 해제
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
	}

	SavedTargetBlock.Reset();
	NotifySkillCastFinished();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_StickyBomb::UpdatePreview()
{
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	APlayerController* PC = (OwnerPawn) ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;

	if (!PC)
	{
		return; // 로깅 생략 (매 프레임 호출됨)
	}

	HighlightBlocks(PreviewBlocks, TAG_Block_Highlight_Preview);

	// 4. 마우스 커서 타겟팅 처리
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Block, true, HitResult);
	AActor* HitActor = HitResult.GetActor();

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
		// 타겟 하이라이트 적용
		TArray<TWeakObjectPtr<AActor>> TargetedActor = { HitActor };
		BatchHighlightBlocks(TargetedActor, TAG_Block_Highlight_Target);
		HighlightedBlock = HitActor;
	}
	else
	{
		HighlightedBlock.Reset();
	}
}

void UGA_StickyBomb::OnLeftClickPressed()
{
	if (HighlightedBlock.IsValid())
	{
		NotifySkillCastStarted();
		SpawnExplosive();
	}
}

void UGA_StickyBomb::OnCancelPressed(float TimeWaited)
{
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UGA_StickyBomb::SpawnExplosive()
{
	if (!CommitAbilityCost(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_StickyBomb: Failed to commit cost"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (!ExplosiveClass)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_StickyBomb: ExplosiveClass is null"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 타겟 블록 정보 백업
	SavedTargetBlock = HighlightedBlock.Get();

	// 타이머 및 프리뷰 즉시 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}
	ClearHighlights(PreviewBlocks);

	// 입력 태스크 및 바인딩 정리
	if (InputTask)
	{
		InputTask->EndTask();
		InputTask = nullptr;
	}

	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn && OwnerPawn->GetController())
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			if (PC->InputComponent)
			{
				for (int32 i = PC->InputComponent->KeyBindings.Num() - 1; i >= 0; --i)
				{
					if (PC->InputComponent->KeyBindings[i].KeyDelegate.GetUObject() == this &&
						PC->InputComponent->KeyBindings[i].Chord.Key == EKeys::LeftMouseButton)
					{
						PC->InputComponent->KeyBindings.RemoveAt(i);
					}
				}
			}
		}
	}

	// 폭발물 생성
	FVector SpawnLoc = OwnerPawn ? OwnerPawn->GetActorLocation() : FVector::ZeroVector;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AExplosive* NewExplosive = GetWorld()->SpawnActor<AExplosive>(ExplosiveClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);

	if (NewExplosive && SavedTargetBlock.IsValid())
	{
		ExplosivesList.Add(NewExplosive);

		if (ExplosivesList.Num() >= 3)
		{
			bIsDetonationReady = true;
		}

		NewExplosive->OnDetonatedDelegate.AddDynamic(this, &UGA_StickyBomb::OnExplosiveDetonated);

		// 데미지 스펙 생성
		FGameplayEffectSpecHandle DamageSpecHandle = MakeRuneDamageEffectSpec(CurrentSpecHandle, CurrentActorInfo);

		// 인터페이스를 통해 위치 정보 획득 (SavedTargetBlock은 이제 AActor*)
		IBlockInfoInterface* BlockInfo = Cast<IBlockInfoInterface>(SavedTargetBlock.Get());
		FVector TargetLoc = BlockInfo ? BlockInfo->GetBlockLocation() : SavedTargetBlock.Get()->GetActorLocation();

		NewExplosive->Initialize(
			SpawnLoc,
			SavedTargetBlock.Get(),
			1.5f,
			AutoDetonateDelay,
			ExplosionRadius * GetRuneModifiedRange(),
			MaxBombCount,
			GetAbilitySystemComponentFromActorInfo(),
			DamageSpecHandle,
			DestructionEffect
		);

		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_StickyBomb: Failed to spawn bomb or invalid target"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UGA_StickyBomb::PerformDetonateAndEnd()
{
	TArray<TWeakObjectPtr<AExplosive>> TempList = ExplosivesList;
	ExplosivesList.Empty();

	bool bAnyDetonated = false;
	for (TWeakObjectPtr<AExplosive>& Explosive : TempList)
	{
		if (Explosive.IsValid())
		{
			Explosive->Detonate();
			bAnyDetonated = true;
		}
	}

	bIsDetonationReady = false;
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_StickyBomb::OnExplosiveDetonated()
{
	ExplosivesList.RemoveAll([](const TWeakObjectPtr<AExplosive>& Ptr) { return !Ptr.IsValid(); });

	if (ExplosivesList.Num() == 0)
	{
		bIsDetonationReady = false;
		CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		UE_LOG(LogTemp, Log, TEXT("GA_StickyBomb: All bombs detonated. Cooldown started."));
	}
}