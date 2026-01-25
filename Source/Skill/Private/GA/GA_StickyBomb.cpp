// Fill out your copyright notice in the Description page of Project Settings.

#include "GA/GA_StickyBomb.h"
#include "Object/Explosive.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "BlockGameplayTags.h"
#include "InputGameplayTags.h"
#include "Collision/CollisionChannels.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

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
		WaitEventTask->EventReceived.AddDynamic(this, &UGA_StickyBomb::OnLeftClickEventReceived);
		WaitEventTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_StickyBomb: Failed to create WaitGameplayEvent task"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
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

	ClearHighlights(PreviewBlockRefs);

	// 입력 태스크 정리
	if (InputTask)
	{
		InputTask->EndTask();
		InputTask = nullptr;
	}

	SavedTargetRef.Reset();
	NotifySkillCastFinished();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_StickyBomb::UpdatePreview()
{
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn) return;
	APlayerController* PC = (OwnerPawn) ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!PC) return;

	HighlightBlocks(PreviewBlockRefs, TAG_Block_Highlight_Preview);

	// 4. 마우스 커서 타겟팅 처리
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Block, true, HitResult);
	FBlockReference HitRef;

	if (HitResult.bBlockingHit)
	{
		// Case A: HISM (청크 내부 블록)
		if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(HitResult.GetComponent()))
		{
			HitRef.TargetObject = HitResult.GetActor(); // Chunk
			HitRef.TargetComponent = HISM;
			HitRef.ItemIndex = HitResult.Item;
		}
		// Case B: Actor (독립 블록)
		else if (AActor* HitActor = HitResult.GetActor())
		{
			HitRef.TargetObject = HitActor;
			HitRef.TargetComponent = nullptr;
			HitRef.ItemIndex = -1;
		}
	}

	// 타겟이 유효하고, 파란색 범위(PreviewBlockRefs) 안에 포함되어 있는지 확인
	// FBlockReference의 operator== 가 구조체 기본 동작으로 비교됨
	if (HitRef.IsValid() && PreviewBlockRefs.Contains(HitRef))
	{
		if (BlockSystem)
		{
			BlockSystem->HighlightBlock(HitRef, TAG_Block_Highlight_Target);
		}
		HighlightedRef = HitRef;
	}
	else
	{
		HighlightedRef.Reset();
	}
}

void UGA_StickyBomb::OnLeftClickEventReceived(FGameplayEventData Payload)
{
	if (HighlightedRef.IsValid())
	{
		NotifySkillCastStarted();
		SpawnExplosive();
	}
	else
	{
		// 프리뷰가 유효하지 않을 때 클릭하면 로그 (디버깅용)
		UE_LOG(LogTemp, Verbose, TEXT("GA_StickyBomb: Clicked but invalid preview"));
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
	SavedTargetRef = HighlightedRef;

	// 타이머 및 프리뷰 즉시 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}
	ClearHighlights(PreviewBlockRefs);

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

	if (NewExplosive && SavedTargetRef.IsValid())
	{
		ExplosivesList.Add(NewExplosive);

		if (ExplosivesList.Num() >= MaxBombCount)
		{
			bIsDetonationReady = true;
		}

		NewExplosive->OnDetonatedDelegate.AddDynamic(this, &UGA_StickyBomb::OnExplosiveDetonated);

		// 데미지 스펙 생성
		FGameplayEffectSpecHandle DamageSpecHandle = MakeRuneDamageEffectSpec(CurrentSpecHandle, CurrentActorInfo);

		// 시스템을 통해 정확한 목표 지점(HISM 인스턴스 위치 등) 계산
		FVector TargetWorldLocation = FVector::ZeroVector;
		AActor* TargetActor = Cast<AActor>(SavedTargetRef.TargetObject.Get());

		if (BlockSystem)
		{
			// 시스템이 HISM 인덱스 혹은 Actor 위치를 계산해서 반환
			TargetWorldLocation = BlockSystem->GetBlockLocation(SavedTargetRef);
			UE_LOG(LogTemp, Verbose, TEXT("GA_Explosive: Calculated TargetWorldLocation %s"), *TargetWorldLocation.ToString());
		}
		else if (TargetActor)
		{
			// 시스템이 없을 경우(거의 없겠지만) 예외 처리
			TargetWorldLocation = TargetActor->GetActorLocation();
		}

		NewExplosive->Initialize(
			SpawnLoc,
			SavedTargetRef,       // FBlockReference
			TargetWorldLocation,  // 정확한 도착 좌표
			1.5f,                 // 투사체 속도 계수 등
			AutoDetonateDelay,    // 자동 기폭 지연 시간
			ExplosionRadius * GetRuneModifiedRange(),
			MaxBombCount,         // 혹은 StackCount
			GetAbilitySystemComponentFromActorInfo(),
			DamageSpecHandle,
			DestructionEffect
		);

		// 투척만 수행하고 능력 종료 (기폭은 나중에 다시 활성화하여 수행)
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
