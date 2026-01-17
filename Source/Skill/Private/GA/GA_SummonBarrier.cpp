// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/GA_SummonBarrier.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/PrimitiveComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "BlockInfoInterface.h"
#include "BlockSpawnInterface.h"
#include "BlockGameplayTags.h"
#include "Collision/CollisionChannels.h"
#include "AbilitySystemComponent.h"

UGA_SummonBarrier::UGA_SummonBarrier()
{
	// 기본값 초기화
	CurrentMovedDistance = 0.0f;
	bIsCharging = false;
	ChargeDirection = FVector::ForwardVector;

	// 초기 그리드 사이즈는 기본값으로 설정하되,
	// UpdatePreview에서 타겟 블록의 정보를 받아 동적으로 갱신됨
	GridSize = 100.0f;
}

void UGA_SummonBarrier::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 변수 초기화
	BarrierPreviewBlocks.Empty();
	SpawnedBlocks.Empty();
	CurrentMovedDistance = 0.0f;
	bIsCharging = false;

	// GridSize는 UpdatePreview에서 마우스 오버된 블록에 맞춰 갱신됨
}

void UGA_SummonBarrier::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTimerHandle);
	}
	ChargeTimerHandle.Invalidate();

	// 프리뷰 액터 정리
	for (TWeakObjectPtr<AActor>& PreviewActor : BarrierPreviewBlocks)
	{
		if (PreviewActor.IsValid())
		{
			PreviewActor.Get()->Destroy();
		}
	}
	BarrierPreviewBlocks.Reset();

	// 바닥 타일 하이라이트 정리
	ClearHighlights(PreviewBlocks);

	// 스킬이 취소/종료되면 남은 블록들도 모두 제거
	for (TWeakObjectPtr<AActor>& Block : SpawnedBlocks)
	{
		if (Block.IsValid())
		{
			Block.Get()->Destroy();
		}
	}

	SpawnedBlocks.Reset();

	// 남은 바인딩 제거
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			// InputComponent에서 이 객체에 바인딩된 모든 키 바인딩 제거
			for (int32 i = PC->InputComponent->KeyBindings.Num() - 1; i >= 0; --i)
			{
				if (PC->InputComponent->KeyBindings[i].KeyDelegate.GetUObject() == this)
				{
					PC->InputComponent->KeyBindings.RemoveAt(i);
				}
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_SummonBarrier::UpdatePreview()
{
	// 건설이 이미 완료되어 대기/돌진 중이라면 프리뷰를 업데이트하지 않음
	if (SpawnedBlocks.Num() > 0) return;

	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_SummonBarrier: OwnerPawn is null in UpdatePreview"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_SummonBarrier: PlayerController is null in UpdatePreview"));
		return;
	}

	HighlightBlocks(PreviewBlocks, TAG_Block_Highlight_Preview);

	// 마우스 커서 타겟팅 및 방벽 프리뷰 계산
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Block, true, HitResult);

	bool bValidTargetFound = false;
	TArray<FTransform> TargetTransforms;

	if (HitResult.bBlockingHit)
	{
		AActor* HitActor = HitResult.GetActor();
		IBlockInfoInterface* HitBlockInfo = Cast<IBlockInfoInterface>(HitActor);

		// 마우스 밑의 블록이 사거리(파란 영역) 안에 포함되어 있을 때만 설치 가능
		bool bIsInPreviewBlocks = false;
		for (const TWeakObjectPtr<AActor>& WeakBlock : PreviewBlocks)
		{
			if (WeakBlock.IsValid() && WeakBlock.Get() == HitActor)
			{
				bIsInPreviewBlocks = true;
				break;
			}
		}
		
		if (HitBlockInfo && bIsInPreviewBlocks)
		{
			bValidTargetFound = true;

			// 인터페이스를 통해 블록 정보 획득 (BlockBase 의존성 제거)
			GridSize = HitBlockInfo->GetBlockGridSize();
			float HalfGridSize = GridSize / 2.0f;
			FVector CenterBaseLocation = HitBlockInfo->GetBlockAlignedLocation() + FVector(0, 0, HalfGridSize) ;
			FVector CurrentPlayerLocation = OwnerPawn->GetActorLocation();

			CalculateBarrierTransforms(CenterBaseLocation, CurrentPlayerLocation, TargetTransforms);
		}
	}

	if (bValidTargetFound)
	{
		UpdateBarrierPreviewActors(TargetTransforms);
	}
	else
	{
		for (auto& Preview : BarrierPreviewBlocks)
		{
			if (Preview.IsValid()) Preview.Get()->SetActorHiddenInGame(true);
		}
	}
}

void UGA_SummonBarrier::CalculateBarrierTransforms(const FVector& CenterLocation, const FVector& PlayerLocation, TArray<FTransform>& OutTransforms)
{
	// Player에서 Center로 향하는 방향 벡터 계산
	FVector Direction = CenterLocation - PlayerLocation;
	FRotator WallRotation;
	FVector RightVector;

	// 방향 결정 로직
	// 생성 지점이 플레이어 동서방향인 경우
	if (FMath::Abs(Direction.X) > FMath::Abs(Direction.Y))
	{
		WallRotation = FRotator(0, 90.0f, 0);
		RightVector = FVector(0, 1.0f, 0); // Y축 방향으로 뻗는 방벽
	}
	// 생성 지점이 플레이어 남북방향인 경우
	else
	{
		WallRotation = FRotator::ZeroRotator;
		RightVector = FVector(1.0f, 0, 0); // X축 방향으로 뻗는 방벽
	}

	OutTransforms.Empty();
	/**
	* [벽 생성 구조 및 오프셋 설명]
	* 기준점(CenterLocation)을 중심으로 좌우 대칭인 3칸 너비, 2층 높이의 벽을 생성합니다.
	* Offsets 배열 { 0, -1, 1 }을 사용하여 기준점으로부터의 가로 위치를 결정합니다.
	*/
	float Offsets[] = { 0.0f, -1.0f, 1.0f };

	// 순회하며 위치 계산
	for (float OffsetMultiplier : Offsets)
	{
		FVector BasePos = CenterLocation + (RightVector * (GridSize * OffsetMultiplier));

		// 1층 추가
		OutTransforms.Add(FTransform(WallRotation, BasePos));

		// 2층 추가
		FVector UpperPos = BasePos + FVector(0, 0, GridSize);
		OutTransforms.Add(FTransform(WallRotation, UpperPos));
	}
}

void UGA_SummonBarrier::UpdateBarrierPreviewActors(const TArray<FTransform>& Transforms)
{
	UWorld* World = GetWorld();
	if (!World) {
		UE_LOG(LogTemp, Warning, TEXT("GA_SummonBarrier: World is null in UpdateBarrierPreviewActors"));
		return;
	}

	if (!PreviewBlockClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_SummonBarrier: PreviewBlockClass is not set."));
		return;
	}

	// 프리뷰 블록은 한 번만 만들어놓고 재사용
	while (BarrierPreviewBlocks.Num() < Transforms.Num())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* NewPreview = World->SpawnActor<AActor>(PreviewBlockClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (NewPreview)
		{
			NewPreview->SetActorEnableCollision(false);
			BarrierPreviewBlocks.Add(NewPreview);
		}
	}

	// 위치 적용 및 점유 확인
	for (int32 i = 0; i < BarrierPreviewBlocks.Num(); ++i)
	{
		if (!BarrierPreviewBlocks[i].IsValid()) continue;
		AActor* Preview = BarrierPreviewBlocks[i].Get();

		// 만들어 놓은 프리뷰 블록이 필요량보다 많으면 숨김
		if (i < Transforms.Num())
		{
			const FTransform& TargetTransform = Transforms[i];
			if (BlockSpawner->IsLocationOccupied(TargetTransform.GetLocation(), GridSize))
			{
				// 소환 할 수 없는 곳은 프리뷰하지 않음
				Preview->SetActorHiddenInGame(true);
			}
			else
			{
				Preview->SetActorTransform(TargetTransform);
				Preview->SetActorHiddenInGame(false);
			}
		}
		else
		{
			Preview->SetActorHiddenInGame(true);
		}
	}
}

void UGA_SummonBarrier::SpawnBlock()
{
	// 비용 및 쿨타임 커밋
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		UE_LOG(LogTemp, Error, TEXT("GA_SummonBarrier: Failed to commit ability"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	if (!BlockSpawner)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_SummonBarrier: Failed to get BlockManagerSubsystem"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 좌클릭 바인딩 해제
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
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

	// 블록 생성
	SpawnedBlocks.Empty();
	FVector AverageLocation = FVector::ZeroVector;
	int32 Count = 0;

	for (TWeakObjectPtr<AActor>& Preview : BarrierPreviewBlocks)
	{
		if (!Preview.IsValid() || Preview.Get()->IsHidden()) continue;

		FVector SpawnLoc = Preview.Get()->GetActorLocation();
		FRotator SpawnRot = Preview.Get()->GetActorRotation();

		// 인터페이스를 통해 파괴 가능한 블록 생성
		AActor* NewBlock = BlockSpawner->SpawnBlockByTag(TAG_Block_Type_Destructible, SpawnLoc, SpawnRot, false);

		if (NewBlock)
		{
			// 생성된 블록 초기화
			NewBlock->SetActorLocationAndRotation(SpawnLoc, SpawnRot);

			// 생성 직후 중력/스냅 로직 끄기 (BlockBase::Tick이 실행되지 않도록 함)
			NewBlock->SetActorTickEnabled(false);

			// 방벽은 움직여야 하므로 Mobility를 Movable로 설정
			// BlockBase를 모르므로 UPrimitiveComponent(엔진 클래스)를 통해 설정
			if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(NewBlock->GetRootComponent()))
			{
				RootPrim->SetMobility(EComponentMobility::Movable);
			}

			// 생성된 블록 목록에 추가
			SpawnedBlocks.Add(NewBlock);
			AverageLocation += SpawnLoc;
			Count++;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GA_SummonBarrier: Failed to spawn block part at %s"), *SpawnLoc.ToString());
		}
	}

	// 프리뷰 정리
	for (auto& Preview : BarrierPreviewBlocks)
	{
		if (Preview.IsValid()) Preview.Get()->Destroy();
	}
	BarrierPreviewBlocks.Empty();
	ClearHighlights(PreviewBlocks);

	// 소환된 블록이 없으면 종료
	if (Count == 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 방벽 건설 완료 -> State.Busy 태그 제거
	NotifySkillCastFinished();

	// 돌진 방향 설정
	if (SpawnedBlocks.Num() > 0 && SpawnedBlocks[0].IsValid())
	{
		FVector PlayerLoc = OwnerPawn->GetActorLocation();
		AverageLocation /= Count;

		// 건설 당시 플레이어 위치를 기준으로 발사 방향 결정
		FVector DirectionToWall = (AverageLocation - PlayerLoc).GetSafeNormal2D();

		if (DirectionToWall.IsZero())
		{
			DirectionToWall = OwnerPawn->GetActorForwardVector().GetSafeNormal2D();
		}

		if (FMath::Abs(DirectionToWall.X) > FMath::Abs(DirectionToWall.Y))
		{
			ChargeDirection = FVector(FMath::Sign(DirectionToWall.X), 0, 0);
		}
		else
		{
			ChargeDirection = FVector(0, FMath::Sign(DirectionToWall.Y), 0);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("GA_SummonBarrier: Wall spawned. Waiting for charge input."));

	// 돌진 입력 대기
	if (WaitInputTask)
	{
		WaitInputTask->EndTask();
	}

	WaitInputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (WaitInputTask)
	{
		WaitInputTask->OnPress.AddDynamic(this, &UGA_SummonBarrier::StartBarrierCharge);
		WaitInputTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_SummonBarrier: Failed to create WaitInputTask for charge"));
	}
}

void UGA_SummonBarrier::StartBarrierCharge(float TimeWaited)
{
	if (bIsCharging) return;

	UE_LOG(LogTemp, Log, TEXT("GA_SummonBarrier: Charge Started!"));

	bIsCharging = true;
	CurrentMovedDistance = 0.0f;

	for (int32 i = 0; i < SpawnedBlocks.Num(); ++i)
	{
		if (!SpawnedBlocks[i].IsValid()) continue;
		AActor* MyBlock = SpawnedBlocks[i].Get();

		// 1. Tick 비활성화 (Grid Snap 방지) - 이미 되어있지만 안전장치
		MyBlock->SetActorTickEnabled(false);

		// 2. 약간 띄우기 (바닥 마찰 방지)
		MyBlock->AddActorWorldOffset(FVector(0, 0, 5.0f), false);

		// 형제 블록 무시 설정 (UPrimitiveComponent 레벨에서 처리)
		if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(MyBlock->GetRootComponent()))
		{
			for (int32 j = 0; j < SpawnedBlocks.Num(); ++j)
			{
				if (i == j) continue; // 자기 자신 제외
				if (SpawnedBlocks[j].IsValid())
				{
					RootPrim->IgnoreActorWhenMoving(SpawnedBlocks[j].Get(), true);
				}
			}
		}
	}

	// 입력 태스크 종료
	if (WaitInputTask)
	{
		WaitInputTask->EndTask();
		WaitInputTask = nullptr;
	}

	// 타이머 시작
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ChargeTimerHandle, this, &UGA_SummonBarrier::TickBarrierCharge, 0.016f, true);
	}
}

void UGA_SummonBarrier::TickBarrierCharge()
{
	if (SpawnedBlocks.Num() == 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	float DeltaTime = 0.016f;
	float MoveDist = ChargeSpeed * DeltaTime;
	FVector DeltaMove = ChargeDirection * MoveDist;

	CurrentMovedDistance += MoveDist;

	// 최대 거리 도달 시 종료(방벽 사라짐)
	if (CurrentMovedDistance >= MaxChargeDistance)
	{
		UE_LOG(LogTemp, Log, TEXT("GA_SummonBarrier: Max distance reached. Destroying wall."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 역순 순회 (삭제 대응)
	for (int32 i = SpawnedBlocks.Num() - 1; i >= 0; --i)
	{
		AActor* Block = SpawnedBlocks[i].Get();

		if (!Block)
		{
			SpawnedBlocks.RemoveAt(i);
			continue;
		}

		FHitResult HitResult;
		// Sweep 이동.
		Block->AddActorWorldOffset(DeltaMove, true, &HitResult);

		if (HitResult.bBlockingHit)
		{
			AActor* HitActor = HitResult.GetActor();

			// 부딪힌 액터가 자기 자신이거나 동료 방벽 블록이면 무시
			bool bIsSpawnedBlock = false;
			for (const TWeakObjectPtr<AActor>& SpawnedBlock : SpawnedBlocks)
			{
				if (SpawnedBlock.IsValid() && SpawnedBlock.Get() == HitActor)
				{
					bIsSpawnedBlock = true;
					break;
				}
			}
			if (bIsSpawnedBlock) continue;
			if (HitActor == GetAvatarActorFromActorInfo()) continue;

			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
			if (TargetASC)
			{
				// SkillBase의 헬퍼 함수를 사용하여 룬 데미지가 적용된 Spec 생성
				FGameplayEffectSpecHandle SpecHandle = MakeRuneDamageEffectSpec(CurrentSpecHandle, CurrentActorInfo);

				if (SpecHandle.IsValid())
				{
					// 시전자의 ASC를 통해 타겟 ASC에게 효과 적용
					GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
						*SpecHandle.Data.Get(),
						TargetASC
					);
				}
			}

			UE_LOG(LogTemp, Log, TEXT("GA_SummonBarrier: Block %s hit obstacle %s"), *Block->GetName(), *GetNameSafe(HitActor));

			// BlockBase::SelfDestroy() 대신 일반 액터 파괴 사용
			Block->Destroy();
			SpawnedBlocks.RemoveAt(i);
		}
	}

	if (SpawnedBlocks.Num() == 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_SummonBarrier::OnCancelPressed(float TimeWaited)
{
	if (SpawnedBlocks.Num() == 0)
	{
		// 방벽이 아직 생성되지 않은 상태이므로 스킬 취소
		Super::OnCancelPressed(TimeWaited);
	}
	else
	{
		// 방벽이 이미 생성된 상태이므로 돌진 시작
		if (!bIsCharging)
		{
			StartBarrierCharge(TimeWaited);
		}
	}
}

bool UGA_SummonBarrier::CanBeCanceled() const
{
	// 방벽이 이미 생성되어 관리 목록에 있다면? (취소 불가)
	if (SpawnedBlocks.Num() > 0)
	{
		return false;
	}

	return Super::CanBeCanceled();
}

void UGA_SummonBarrier::OnLeftClickPressed()
{
	// BarrierPreviewBlocks 배열 중 하나라도 유효하고(보이고) 있다면 설치 가능한 상태로 간주
	bool bCanSpawn = false;

	for (const auto& Preview : BarrierPreviewBlocks)
	{
		if (Preview.IsValid() && !Preview.Get()->IsHidden())
		{
			bCanSpawn = true;
			break;
		}
	}

	if (bCanSpawn)
	{
		// 실제 스킬 시전 시작 알림
		NotifySkillCastStarted();
		// 블록 생성 시도
		SpawnBlock();
	}
}