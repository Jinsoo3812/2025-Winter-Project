// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/GA_BuffBarrier.h"
#include "Block/BlockBase.h"
#include "Interface/IAttributeSetProvider.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"

// 페이즈 관리를 위한 태그 정의
UE_DEFINE_GAMEPLAY_TAG(TAG_BuffBarrier_Phase1, "State.Skill.BuffBarrier.Phase1");
UE_DEFINE_GAMEPLAY_TAG(TAG_BuffBarrier_Phase2, "State.Skill.BuffBarrier.Phase2");

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



	// 현재 ASC에 붙어있는 태그를 기반으로 페이즈 결정
	if (ASC->HasMatchingGameplayTag(TAG_BuffBarrier_Phase2))
	{
		// 2페이즈 태그가 있으면 -> 3단계(해제) 실행
		ExecutePhase3_Cleanup();
	}
	else if (ASC->HasMatchingGameplayTag(TAG_BuffBarrier_Phase1))
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
	// 1. 비용 지불 (첫 시전시에만 비용 소모, 쿨타임은 나중에)
	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::ExecutePhase1 - Failed to commit cost"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 2. 데이터 초기화 (재사용 시 안전장치)
	HighlightedBlocks.Empty();
	if (SpawnedWalls.Num() > 0)
	{
		for (auto& Wall : SpawnedWalls)
		{
			if (Wall && IsValid(Wall)) Wall->Destroy();
		}
		SpawnedWalls.Empty();
	}

	InstallCenterLocation = GetAvatarActorFromActorInfo()->GetActorLocation();

	// 3. 범위 계산 및 블록 수집
	// Rune modifier 적용된 범위를 계산하여 RangeXY 업데이트 (RangeZ는 건드리지 않음)
	float OriginalRange = RangeXY;
	RangeXY *= GetRuneModifiedRange(); // 룬 적용
	// RangeXY 디버그 출력
	UE_LOG(LogTemp, Log, TEXT("GA_BuffBarrier::ExecutePhase1 - Modified RangeXY: %f"), RangeXY);

	// 부모 클래스의 FindBlocksInRange 사용
	TArray<AActor*> ActorsInRange;
	FindBlocksInRange(ActorsInRange);
	
	// AActor*를 ABlockBase*로 변환
	for (AActor* Actor : ActorsInRange)
	{
		if (ABlockBase* Block = Cast<ABlockBase>(Actor))
		{
			HighlightedBlocks.Add(Block);
		}
	}

	// RangeXY 복구 (다음 프레임을 위해 원상복구)
	RangeXY = OriginalRange;

	if (HighlightedBlocks.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::ExecutePhase1 - No blocks found in range"));
		// 블록이 없어도 로직상 태그는 붙여서 다음 단계로 갈지, 종료할지 결정해야 함.
		// 여기서는 블록이 없으면 의미가 없으므로 즉시 종료
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 4. 하이라이트 적용 (Preview 색상)
	// HighlightedBlocks를 AActor* 배열로 변환
	TArray<AActor*> ActorsToHighlight;
	for (ABlockBase* Block : HighlightedBlocks)
	{
		if (Block)
		{
			ActorsToHighlight.Add(Block);
		}
	}
	BatchHighlightBlocks(ActorsToHighlight, 1.0f);

	// 5. 태그 부착 (Phase 1 시작 알림)
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->AddLooseGameplayTag(TAG_BuffBarrier_Phase1);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ExecutePhase1 - ASC is null, cannot add tag"));
	}

	// 6. 자동 전환 타이머 설정
	if (UWorld* World = GetWorld())
	{
		// 기존 타이머가 있다면 정리
		World->GetTimerManager().ClearTimer(AutoTransitionTimerHandle);

		// 타이머 시간 유효성 체크 (0초면 즉시 실행되어 꼬일 수 있음)
		float WaitTime = (AutoTransitionTime > 0.0f) ? AutoTransitionTime : 0.1f;

		// WeakObjectPtr를 사용하여 Ability 종료 후에도 타이머가 안전하게 호출되도록 함
		TWeakObjectPtr<UGA_BuffBarrier> WeakThis(this);
		World->GetTimerManager().SetTimer(AutoTransitionTimerHandle, [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnAutoTransition();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::AutoTransition - Ability object is no longer valid"));
			}
		}, WaitTime, false);
		
		UE_LOG(LogTemp, Log, TEXT("GA_BuffBarrier::ExecutePhase1 - Timer Set for %.2f seconds"), WaitTime);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ExecutePhase1 - World is null"));
	}

	UE_LOG(LogTemp, Log, TEXT("GA_BuffBarrier: Phase 1 Highlighted %d blocks"), HighlightedBlocks.Num());

	// 7. 스킬 종료 (태그는 남기고 생명주기는 종료하여 다른 행동 가능하게 함)
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BuffBarrier::ExecutePhase2_Deploy()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();

	// 1. 타이머 및 이전 태그 정리
	if (World)
	{
		World->GetTimerManager().ClearTimer(AutoTransitionTimerHandle);
	}

	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(TAG_BuffBarrier_Phase1);
		ASC->AddLooseGameplayTag(TAG_BuffBarrier_Phase2);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ExecutePhase2 - ASC is null"));
	}

	// 2. 가장자리 블록 찾기
	TArray<ABlockBase*> EdgeBlocks;
	FindEdgeBlocks(HighlightedBlocks, EdgeBlocks);

	// 3. 울타리 생성
	if (WallBlockClass && World)
	{
		for (ABlockBase* BaseBlock : EdgeBlocks)
		{
			if (!BaseBlock) continue;

			// 기준 블록 바로 위에 생성
			FVector SpawnLoc = BaseBlock->GetActorLocation() + FVector(0, 0, BaseBlock->GetGridSize());
			ABlockBase* NewWall = ABlockBase::SpawnBlock(World, WallBlockClass, SpawnLoc, false);

			if (NewWall)
			{
				SpawnedWalls.Add(NewWall);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::ExecutePhase2 - Failed to spawn wall at %s"), *SpawnLoc.ToString());
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ExecutePhase2 - WallBlockClass or World is null"));
	}

	// 4. 버프 적용
	ApplyBuffToTargets();

	// 5. 자동 전환 타이머 재설정 (3단계로 넘어가기 위함)
	if (World)
	{
		TWeakObjectPtr<UGA_BuffBarrier> WeakThis(this);
		World->GetTimerManager().SetTimer(AutoTransitionTimerHandle, [WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnAutoTransition();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::AutoTransition - Ability object is no longer valid"));
			}
		}, AutoTransitionTime, false);
	}

	UE_LOG(LogTemp, Log, TEXT("GA_BuffBarrier: Phase 2 Deployed %d walls"), SpawnedWalls.Num());

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BuffBarrier::ExecutePhase3_Cleanup()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();

	// 1. 타이머 및 태그 정리
	if (World)
	{
		World->GetTimerManager().ClearTimer(AutoTransitionTimerHandle);
	}

	if (ASC)
	{
		ASC->RemoveLooseGameplayTag(TAG_BuffBarrier_Phase2);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ExecutePhase3 - ASC is null"));
	}

	// 2. 벽 제거
	for (ABlockBase* Wall : SpawnedWalls)
	{
		if (Wall && IsValid(Wall))
		{
			Wall->Destroy();
		}
	}
	SpawnedWalls.Empty();

	// 3. 바닥 하이라이트 복구
	// HighlightedBlocks를 AActor* 배열로 변환
	TArray<AActor*> ActorsToReset;
	for (ABlockBase* Block : HighlightedBlocks)
	{
		if (Block)
		{
			ActorsToReset.Add(Block);
		}
	}
	BatchHighlightBlocks(ActorsToReset, 0.0f);
	HighlightedBlocks.Empty();

	// 4. 쿨타임 적용 (모든 시퀀스가 끝난 시점에서 적용)
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);

	UE_LOG(LogTemp, Log, TEXT("GA_BuffBarrier: Phase 3 Cleanup complete"));

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_BuffBarrier::OnAutoTransition()
{
	UE_LOG(LogTemp, Log, TEXT("GA_BuffBarrier::OnAutoTransition - Timer Fired"));

	// CachedASC 유효성 검사
	if (CachedASC.IsValid())
	{
		// 다시 활성화 시도
		bool bSuccess = CachedASC->TryActivateAbility(CurrentSpecHandle);

		if (!bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier::OnAutoTransition - Failed to auto-activate ability."));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("GA_BuffBarrier::OnAutoTransition - Successfully reactivated for next phase"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::OnAutoTransition - Cached ASC is invalid or expired"));
	}
}

void UGA_BuffBarrier::FindEdgeBlocks(const TArray<ABlockBase*>& InBlocks, TArray<ABlockBase*>& OutEdges)
{
	OutEdges.Empty();
	if (InBlocks.Num() == 0) return;

	float GridSize = InBlocks[0]->GetGridSize();
	float Tolerance = 5.0f;

	// [최적화] O(N^2)를 방지하기 위해 위치 조회를 위한 Set 생성 (O(N))
	// FVector를 Key로 쓰기 위해 약간의 오차를 고려해야 하지만, 블록이 그리드에 정렬되어 있다고 가정
	TSet<FVector> BlockLocations;
	BlockLocations.Reserve(InBlocks.Num());

	for (ABlockBase* Block : InBlocks)
	{
		if (Block)
		{
			BlockLocations.Add(Block->GetActorLocation());
		}
	}

	// 4방향 (상하좌우)
	FVector Directions[] = { FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0), FVector(0,-1,0) };

	for (ABlockBase* Block : InBlocks)
	{
		if (!Block) continue;

		FVector MyLoc = Block->GetActorLocation();
		int32 NeighborCount = 0;

		for (const FVector& Dir : Directions)
		{
			FVector CheckLoc = MyLoc + (Dir * GridSize);

			// [최적화] 이중 반복문 대신 Set에서 검색 (O(1))
			// 부동소수점 오차를 고려해야 한다면 FIntVector 변환이나 별도 처리가 필요할 수 있으나,
			// 그리드 시스템이 정확하다면 Set.Contains로 충분함. 
			// 만약 Set 검색이 실패한다면, Tolerance 범위 내 검색 로직으로 대체 필요할 수 있음.
			// 여기서는 기존 로직의 Tolerance(5.0f)를 고려하여 Set 검색 대신 FindClosestKey 등을 쓰거나,
			// 단순히 그리드 좌표가 정확하다고 가정하고 Contains를 사용.
			if (BlockLocations.Contains(CheckLoc))
			{
				NeighborCount++;
			}
			else
			{
				// 정확히 일치하지 않는 경우를 대비해, Set 순회 없이 
				// 현재 시스템상 블록들이 정수 단위 그리드에 딱 맞게 배치된다면 위 Contains로 충분함.
				// 만약 미세한 오차가 있다면 기존 방식을 써야하나, 여기서는 최적화를 위해 좌표 정합성을 가정함.
			}
		}

		// 4면 중 하나라도 뚫려있으면(이웃이 없으면) 가장자리로 판정
		if (NeighborCount < 4)
		{
			OutEdges.Add(Block);
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

	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_BuffBarrier::ApplyBuffToTargets - OwnerPawn is null"));
		return;
	}

	FVector CenterLoc = InstallCenterLocation;

	// 범위 재계산 (Phase 1에서 썼던 값과 동일해야 함)
	float Radius = GetRuneModifiedRange() * RangeXY;

	// 구체 형태의 오버랩 검사
	TArray<FOverlapResult> OverlapResults;
	FCollisionShape OverlapShape = FCollisionShape::MakeSphere(Radius);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn); // Pawn 타입만 1차 필터링

	FCollisionQueryParams QueryParams;

	bool bOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		CenterLoc,
		FQuat::Identity,
		ObjectQueryParams,
		OverlapShape,
		QueryParams
	);

	if (bOverlap)
	{
		// [추가] 2. 이미 처리된 액터를 걸러내기 위한 집합(Set)
		// OverlapMulti는 컴포넌트 단위로 검출되므로(캡슐, 메쉬 등), 한 액터가 여러 번 잡히는 것을 방지
		TSet<AActor*> ProcessedActors;

		for (const FOverlapResult& Result : OverlapResults)
		{
			AActor* TargetActor = Result.GetActor();
			if (!TargetActor) continue;

			// [추가] 3. 중복 처리 방지
			if (ProcessedActors.Contains(TargetActor))
			{
				continue;
			}
			ProcessedActors.Add(TargetActor);

			// 1. 유효성 검사: IAttributeSetProvider 인터페이스 구현 여부 확인
			// 이것을 통해 일반 액터나 배경 등을 거를 수 있음
			if (!TargetActor->Implements<UAttributeSetProvider>())
			{
				continue;
			}

			// 2. ASC 가져오기
			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
			if (TargetASC)
			{
				// 2. TAG_Player 태그를 가지고 있는지 확인
				if (TargetASC->HasMatchingGameplayTag(TAG_Player))
				{
					// 3. 버프 적용
					FGameplayEffectContextHandle Context = MakeEffectContext(CurrentSpecHandle, CurrentActorInfo);
					FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(BuffEffectClass, GetAbilityLevel(), Context);

					if (SpecHandle.IsValid())
					{

						FGameplayTag MagnitudeTag = FGameplayTag::RequestGameplayTag(FName("Data.Skill.Damage"));
						SpecHandle.Data.Get()->SetSetByCallerMagnitude(MagnitudeTag, GetRuneModifiedDamage());
						GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

						// 버프 적용 확인용 로그
						UE_LOG(LogTemp, Log, TEXT("GA_BuffBarrier: Applied Buff [%s] to Player Target [%s]"),
							*BuffEffectClass->GetName(), *TargetActor->GetName());
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier: Failed to create SpecHandle for target %s"), *TargetActor->GetName());
					}
				}
				else
				{
					// 플레이어 태그가 없는 경우 로그
					UE_LOG(LogTemp, Log, TEXT("GA_BuffBarrier: Target %s has ASC but no Player tag, skipping."), *TargetActor->GetName());
				}
			}
			else
			{
				// ASC가 없는 경우 로그
				UE_LOG(LogTemp, Warning, TEXT("GA_BuffBarrier: Target %s has no ASC"), *TargetActor->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("GA_BuffBarrier: No targets found for buff within radius %f"), Radius);
	}
}

bool UGA_BuffBarrier::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// ActorInfo나 ASC가 유효하다면 태그 확인
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		// 1페이즈나 2페이즈 태그가 붙어있다면(이미 스킬 진행 중), 비용 검사를 패스(true)
		if (ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(TAG_BuffBarrier_Phase1) ||
			ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(TAG_BuffBarrier_Phase2))
		{
			return true;
		}
	}

	// 그 외(첫 시전)에는 기본 비용 검사 수행
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}