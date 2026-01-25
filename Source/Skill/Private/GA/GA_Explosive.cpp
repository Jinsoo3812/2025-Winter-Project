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
#include "BlockSystemInterface.h" 
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

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

void UGA_Explosive::UpdatePreview()
{
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn) return;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) return;

	// 프리뷰 하이라이트 초기화 및 재생성
	HighlightBlocks(PreviewBlockRefs, TAG_Block_Highlight_Preview);

	// 마우스 커서 위치의 블록 타겟팅 처리
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Block, true, HitResult);
	FBlockReference HitRef;

	//  HISM 및 Actor 구분하여 참조 생성
	if (HitResult.bBlockingHit)
	{
		// HISM의 경우
		if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(HitResult.GetComponent()))
		{
			HitRef.TargetObject = HitResult.GetActor(); // HISM의 소유 Chunk
			HitRef.TargetComponent = HISM; // HISM 컴포넌트
			HitRef.ItemIndex = HitResult.Item; // 히트된 블록 인스턴스의 인덱스
		}
		// Actor 블록의 경우
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

void UGA_Explosive::OnLeftClickEventReceived(FGameplayEventData Payload)
{
	if (HighlightedRef.IsValid())
	{
		NotifySkillCastStarted();
		SpawnExplosive();
	}
	else
	{
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
	SavedTargetRef = HighlightedRef;

	// 타이머 및 프리뷰 정리
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
		FGameplayEffectSpecHandle DamageSpecHandle = MakeRuneDamageEffectSpec(CurrentSpecHandle, CurrentActorInfo);

		// HISM 지원을 위해 정확한 타겟 위치 계산
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
			SavedTargetRef,       // FBlockReference 전달
			TargetWorldLocation,  // 정확히 계산된 도착 위치 전달
			1.0f,
			0.01f,
			ExplosionRadius * GetRuneModifiedRange(),
			1,
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
