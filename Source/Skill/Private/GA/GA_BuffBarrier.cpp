// Fill out your copyright notice in the Description page of Project Settings.

#include "GA/GA_BuffBarrier.h"
#include "BlockInfoInterface.h"
#include "BlockSpawnInterface.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"
#include "BlockGameplayTags.h"
#include "SkillGameplayTags.h"

UGA_BuffBarrier::UGA_BuffBarrier()
{
	// 객체 생성 정책: 멤버 변수(HighlightedBlocks, SpawnedWalls) 유지를 위해
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_BuffBarrier::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ActivateAbility - ASC is null"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	else {
		CachedASC = ASC;
	}

	if (UWorld* World = GetWorld())
	{
		if (!BlockSpawner) {
			// BlockSpawner 캐싱
			BlockSpawner = IBlockSpawnInterface::GetBlockManagerSubsystem(World);
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ActivateAbility - World is null"));
	}

	// 현재 ASC에 붙어있는 태그를 기반으로 페이즈 결정
	if (ASC->HasMatchingGameplayTag(TAG_Skill_BuffBarrier_Phase2))
	{
		// 2페이즈 태그가 있으면 -> 3단계(해제) 실행
		ExecutePhase3_Cleanup();
	}
	else if (ASC->HasMatchingGameplayTag(TAG_Skill_BuffBarrier_Phase1))
	{
		// 1페이즈 태그가 있으면 -> 2단계(설치) 실행
		ExecutePhase2_Deploy();
	}
	else
	{
		// 태그가 없으면 -> 1단계(시작) 실행
		ExecutePhase1_Highlight();
	}
}

void UGA_BuffBarrier::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 부모 클래스의 EndAbility 호출 (State.Casting 태그 제거 등)
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BuffBarrier::ExecutePhase1_Highlight()
{
	// 비용 지불 (첫 시전시에만 비용 소모, 쿨타임은 나중에)
	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::ExecutePhase1 - Failed to commit cost"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	ClearHighlights(HighlightedBlocks);
	if (SpawnedWalls.Num() > 0)
	{
		for (AActor* Wall : SpawnedWalls)
		{
			if (IsValid(Wall)) Wall->Destroy();
		}
		SpawnedWalls.Empty();
	}

	InstallCenterLocation = GetAvatarActorFromActorInfo()->GetActorLocation();

	// 범위 계산 및 블록 수집
	float OriginalRange = RangeXY;
	RangeXY *= GetRuneModifiedRange(); // 룬 적용

	FindBlocksInRange(HighlightedBlocks);

	// 인터페이스 구현 여부로 필터링하여 보관
	for (AActor* Actor : HighlightedBlocks)
	{
		if (IsValid(Actor) && Actor->Implements<UBlockInfoInterface>())
		{
			HighlightedBlocks.Add(Actor);
		}
	}

	// RangeXY 복구
	RangeXY = OriginalRange;

	if (HighlightedBlocks.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::ExecutePhase1 - No blocks found in range"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 하이라이트 적용
	BatchHighlightBlocks(HighlightedBlocks, TAG_Block_Highlight_Preview);

	// 태그 부착 (Phase 1 시작 알림)
	if (CachedASC.IsValid())
	{
		CachedASC->AddLooseGameplayTag(TAG_Skill_BuffBarrier_Phase1);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ExecutePhase1 - ASC is invalid"));
	}

	// 자동 전환 타이머 설정
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoTransitionTimerHandle);
		float WaitTime = (AutoTransitionTime > 0.0f) ? AutoTransitionTime : 0.1f;

		// 어떤 이유로 GA 객체가 사라졌을 때를 대비한 TWeakObjectPtr 방어 프로그래밍
		// InstnacedPerActor 정책이므로 GA 객체가 유지되긴 하지만 안전을 위해
		TWeakObjectPtr<UGA_BuffBarrier> WeakThis(this);
		World->GetTimerManager().SetTimer(AutoTransitionTimerHandle, [WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->OnAutoTransition();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::AutoTransition - Ability object is invalid"));
				}
			}, WaitTime, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ExecutePhase1 - World is null"));
	}

	// 스킬 종료 (태그는 남김)
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BuffBarrier::ExecutePhase2_Deploy()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();

	if (World)
	{
		World->GetTimerManager().ClearTimer(AutoTransitionTimerHandle);
	}

	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(TAG_Skill_BuffBarrier_Phase1);
		ASC->AddLooseGameplayTag(TAG_Skill_BuffBarrier_Phase2);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ExecutePhase2 - ASC is null"));
	}

	// 1. 가장자리 블록 찾기
	TArray<TObjectPtr<AActor>> EdgeBlocks;
	FindEdgeBlocks(HighlightedBlocks, EdgeBlocks);

	// 2. 울타리 생성
	if (BlockSpawner)
	{
		for (AActor* BaseBlock : EdgeBlocks)
		{
			IBlockInfoInterface* BlockInfo = Cast<IBlockInfoInterface>(BaseBlock);
			if (!BlockInfo) continue;

			// 인터페이스를 통해 위치 및 그리드 사이즈 획득
			FVector SpawnLoc = BlockInfo->GetBlockLocation() + FVector(0, 0, BlockInfo->GetBlockGridSize());

			// 태그를 사용하여 블록 생성
			AActor* NewWall = BlockSpawner->SpawnBlockByTag(TAG_Block_Type_Destructible, SpawnLoc, FRotator::ZeroRotator, false);

			if (NewWall)
			{
				SpawnedWalls.Add(NewWall);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::ExecutePhase2 - Failed to spawn wall via tag"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ExecutePhase2 - BlockSpawner is null"));
	}

	// 버프 적용
	ApplyBuffToTargets();

	// 자동 전환 타이머 재설정
	if (World)
	{
		World->GetTimerManager().ClearTimer(AutoTransitionTimerHandle);
		float WaitTime = (AutoTransitionTime > 0.0f) ? AutoTransitionTime : 0.1f;

		// 어떤 이유로 GA 객체가 사라졌을 때를 대비한 TWeakObjectPtr 방어 프로그래밍
		// InstnacedPerActor 정책이므로 GA 객체가 유지되긴 하지만 안전을 위해
		TWeakObjectPtr<UGA_BuffBarrier> WeakThis(this);
		World->GetTimerManager().SetTimer(AutoTransitionTimerHandle, [WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->OnAutoTransition();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::AutoTransition - Ability object is invalid"));
				}
			}, WaitTime, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ExecutePhase1 - World is null"));
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BuffBarrier::ExecutePhase3_Cleanup()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();

	if (World)
	{
		World->GetTimerManager().ClearTimer(AutoTransitionTimerHandle);
	}

	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(TAG_Skill_BuffBarrier_Phase2);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ExecutePhase3 - ASC is null"));
	}

	// 생성된 벽 제거
	for (AActor* Wall : SpawnedWalls)
	{
		if (IsValid(Wall))
		{
			Wall->Destroy();
		}
	}
	SpawnedWalls.Empty();

	// 바닥 하이라이트 제거
	ClearHighlights(HighlightedBlocks);

	// 쿨타임 적용
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BuffBarrier::OnAutoTransition()
{
	if (CachedASC.IsValid())
	{
		bool bSuccess = CachedASC->TryActivateAbility(CurrentSpecHandle);
		if (!bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::OnAutoTransition - Failed to auto-activate"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::OnAutoTransition - Cached ASC is invalid"));
	}
}

void UGA_BuffBarrier::FindEdgeBlocks(const TArray<TObjectPtr<AActor>>& InBlocks, TArray<TObjectPtr<AActor>>& OutEdges)
{
	OutEdges.Empty();
	if (InBlocks.Num() == 0) return;

	// 첫 번째 블록에서 그리드 사이즈 획득 (인터페이스 사용)
	IBlockInfoInterface* FirstBlock = Cast<IBlockInfoInterface>(InBlocks[0]);
	if (!FirstBlock) return;

	float GridSize = FirstBlock->GetBlockGridSize();

	TSet<FVector> BlockLocations;
	for (AActor* Actor : InBlocks)
	{
		if (Actor)
		{
			BlockLocations.Add(Actor->GetActorLocation());
		}
	}

	FVector Directions[] = { FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0), FVector(0,-1,0) };

	for (AActor* Actor : InBlocks)
	{
		IBlockInfoInterface* Info = Cast<IBlockInfoInterface>(Actor);
		if (!Info) continue;

		FVector MyLoc = Info->GetBlockLocation();
		int32 NeighborCount = 0;

		for (const FVector& Dir : Directions)
		{
			FVector CheckLoc = MyLoc + (Dir * GridSize);
			if (BlockLocations.Contains(CheckLoc))
			{
				NeighborCount++;
			}
		}

		if (NeighborCount < 4)
		{
			OutEdges.Add(Actor);
		}
	}
}

void UGA_BuffBarrier::ApplyBuffToTargets()
{
	if (!BuffEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::ApplyBuffToTargets - BuffEffectClass is null"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ApplyBuffToTargets - World is null"));
		return;
	}

	FVector CenterLoc = InstallCenterLocation;
	float Radius = GetRuneModifiedRange() * RangeXY;

	TArray<FOverlapResult> OverlapResults;
	FCollisionShape OverlapShape = FCollisionShape::MakeSphere(Radius);
	FCollisionObjectQueryParams ObjectQueryParams(ECC_Pawn);
	FCollisionQueryParams QueryParams;

	bool bOverlap = World->OverlapMultiByObjectType(OverlapResults, CenterLoc, FQuat::Identity, ObjectQueryParams, OverlapShape, QueryParams);

	if (bOverlap)
	{
		TSet<AActor*> ProcessedActors;
		for (const FOverlapResult& Result : OverlapResults)
		{
			AActor* TargetActor = Result.GetActor();
			if (!TargetActor || ProcessedActors.Contains(TargetActor)) continue;

			ProcessedActors.Add(TargetActor);

			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
			if (TargetASC && TargetASC->HasMatchingGameplayTag(TAG_Player))
			{
				FGameplayEffectContextHandle Context = MakeEffectContext(CurrentSpecHandle, CurrentActorInfo);
				FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(BuffEffectClass, GetAbilityLevel(), Context);

				if (SpecHandle.IsValid())
				{
					SpecHandle.Data.Get()->SetSetByCallerMagnitude(TAG_Data_Damage, GetRuneModifiedDamage());
					GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
				}
				else {
					UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::ApplyBuffToTargets - Failed to create SpecHandle"));
				}
			}
			else {
				// ASC가 없거나 플레이어가 아닌 경우 무시
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("GA_BuffBarrier: No targets found in radius"));
	}
}

bool UGA_BuffBarrier::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (ActorInfo->AbilitySystemComponent->HasAnyMatchingGameplayTags(
			FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{TAG_Skill_BuffBarrier_Phase1, TAG_Skill_BuffBarrier_Phase2})))
		{
			return true;
		}
	}
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}