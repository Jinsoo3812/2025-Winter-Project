// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/GA_SummonBarrier.h"
#include "Block/DestructibleBlock.h"
#include "Block/BlockBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"
#include "Components/StaticMeshComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"

UGA_SummonBarrier::UGA_SummonBarrier()
{
	// 기본 사거리 설정
	ConstructionRangeXY = 600.0f;

	// 기본값 초기화
	CurrentMovedDistance = 0.0f;
	bIsCharging = false;
	ChargeDirection = FVector::ForwardVector;
}

void UGA_SummonBarrier::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 변수 초기화
	BarrierPreviewBlocks.Empty();
	SpawnedBlocks.Empty();
	CurrentMovedDistance = 0.0f;
	bIsCharging = false;
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
	for (TObjectPtr<AActor>& PreviewActor : BarrierPreviewBlocks)
	{
		if (PreviewActor && IsValid(PreviewActor))
		{
			PreviewActor->Destroy();
		}
	}
	BarrierPreviewBlocks.Empty();

	// 돌진 중 스킬이 취소/종료되면 남은 블록들도 모두 제거 (관리 책임이 스킬에 있음)
	for (TObjectPtr<ADestructibleBlock>& Block : SpawnedBlocks)
	{
		if (Block && IsValid(Block))
		{
			Block->Destroy();
		}
	}

	SpawnedBlocks.Empty();

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

	// 위치 변경 시 하이라이트 갱신 (부모 로직)
	FVector CurrentPlayerLocation = OwnerPawn->GetActorLocation();
	if (!LastPlayerLocation.Equals(CurrentPlayerLocation, 10.0f))
	{
		ClearHighlights();
		HighlightBlocksInRange();
		LastPlayerLocation = CurrentPlayerLocation;
	}

	// 마우스 커서 타겟팅
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Visibility, true, HitResult);

	bool bValidTargetFound = false;
	TArray<FTransform> TargetTransforms;

	if (HitResult.bBlockingHit)
	{
		ABlockBase* HitBlock = Cast<ABlockBase>(HitResult.GetActor());
		if (HitBlock && HighlightedBlocks.Contains(HitBlock))
		{
			bValidTargetFound = true;
			// 타겟 블록 위쪽을 기준으로 계산
			FVector CenterBaseLocation = HitBlock->GetActorLocation() + FVector(0, 0, BlockSize);
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
			if (Preview) Preview->SetActorHiddenInGame(true);
		}
	}
}

void UGA_SummonBarrier::CalculateBarrierTransforms(const FVector& CenterLocation, const FVector& PlayerLocation, TArray<FTransform>& OutTransforms)
{
	FVector Direction = CenterLocation - PlayerLocation;
	FRotator WallRotation;
	FVector RightVector;

	// 방향 결정 로직
	if (FMath::Abs(Direction.X) > FMath::Abs(Direction.Y))
	{
		WallRotation = FRotator(0, 90.0f, 0);
		RightVector = FVector(0, 1.0f, 0); // Y축
	}
	else
	{
		WallRotation = FRotator::ZeroRotator;
		RightVector = FVector(1.0f, 0, 0); // X축
	}

	OutTransforms.Empty();
	float Offsets[] = { 0.0f, -1.0f, 1.0f };

	for (float OffsetMultiplier : Offsets)
	{
		FVector BasePos = CenterLocation + (RightVector * (BlockSize * OffsetMultiplier));
		// 1층
		OutTransforms.Add(FTransform(WallRotation, BasePos));
		// 2층
		FVector UpperPos = BasePos + FVector(0, 0, BlockSize);
		OutTransforms.Add(FTransform(WallRotation, UpperPos));
	}
}

void UGA_SummonBarrier::UpdateBarrierPreviewActors(const TArray<FTransform>& Transforms)
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (!PreviewBlockClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_SummonBarrier: PreviewBlockClass is not set."));
		return;
	}

	// 풀링(Pooling) 방식으로 개수 맞추기
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
		AActor* Preview = BarrierPreviewBlocks[i];
		if (!Preview) continue;

		if (i < Transforms.Num())
		{
			const FTransform& TargetTransform = Transforms[i];
			if (IsLocationOccupied(TargetTransform.GetLocation()))
			{
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

bool UGA_SummonBarrier::IsLocationOccupied(const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_SummonBarrier: World is null in IsLocationOccupied"));
		return true;
	}

	FVector BoxExtent = FVector(BlockSize * 0.4f, BlockSize * 0.4f, BlockSize * 0.4f);
	FCollisionShape CheckShape = FCollisionShape::MakeBox(BoxExtent);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams;
	for (const auto& Preview : BarrierPreviewBlocks)
	{
		if (Preview) QueryParams.AddIgnoredActor(Preview);
	}

	TArray<FOverlapResult> OverlapResults;
	return World->OverlapMultiByObjectType(OverlapResults, Location, FQuat::Identity, ObjectQueryParams, CheckShape, QueryParams);
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

	if (!BlockToSpawn)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_SummonBarrier: BlockToSpawn is not set"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// 기존 프리뷰 제거 및 좌클릭 바인딩 해제
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			// 좌클릭 바인딩 제거
			for (int32 i = PC->InputComponent->KeyBindings.Num() - 1; i >= 0; --i)
			{
				if (PC->InputComponent->KeyBindings[i].KeyDelegate.GetUObject() == this &&
					PC->InputComponent->KeyBindings[i].Chord.Key == EKeys::LeftMouseButton)
				{
					PC->InputComponent->KeyBindings.RemoveAt(i);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("GA_SummonBarrier: PC or InputComponent is null"));
		}
	}

	// 1. 블록 생성
	SpawnedBlocks.Empty();
	FVector AverageLocation = FVector::ZeroVector;
	int32 Count = 0;

	for (TObjectPtr<AActor>& Preview : BarrierPreviewBlocks)
	{
		if (!Preview || Preview->IsHidden()) continue;

		FVector SpawnLoc = Preview->GetActorLocation();
		FRotator SpawnRot = Preview->GetActorRotation();

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ADestructibleBlock* NewBlock = World->SpawnActor<ADestructibleBlock>(BlockToSpawn, SpawnLoc, SpawnRot, SpawnParams);
		if (NewBlock)
		{
			NewBlock->SpawnBlock(SpawnLoc, EBlockType::Destructible);

			// 생성 직후 중력/스냅 로직 끄기
			NewBlock->SetActorTickEnabled(false);

			// [추가] 방벽을 구성하는 블록은 지지대가 없어져도 스스로 추락하지 않도록 설정
			// (BlockBase.h에 SetCanFall 함수 추가 필요)
			NewBlock->SetCanFall(false);

			// 이동 가능 설정
			if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(NewBlock->GetRootComponent()))
			{
				RootPrim->SetMobility(EComponentMobility::Movable);
			}

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
		if (Preview) Preview->Destroy();
	}
	BarrierPreviewBlocks.Empty();
	ClearHighlights();

	if (Count == 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 2. 돌진 방향 설정
	if (SpawnedBlocks.Num() > 0 && SpawnedBlocks[0])
	{
		FVector PlayerLoc = OwnerPawn->GetActorLocation();
		AverageLocation /= Count;
		FVector DirectionToWall = (AverageLocation - PlayerLoc).GetSafeNormal2D();

		// 방향이 0(플레이어와 겹침)이면 시선 방향
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

	// 3. 재입력 대기
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
		ADestructibleBlock* MyBlock = SpawnedBlocks[i];
		if (!MyBlock || !IsValid(MyBlock)) continue;

		// 1. Tick 비활성화 (Grid Snap 방지)
		MyBlock->SetActorTickEnabled(false);

		// 2. 약간 띄우기 (바닥 마찰 방지)
		MyBlock->AddActorWorldOffset(FVector(0, 0, 5.0f), false);

		// 3. [수정] 형제 블록 무시 설정
		// AActor가 아닌 RootComponent(UPrimitiveComponent) 수준에서 설정해야 함
		if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(MyBlock->GetRootComponent()))
		{
			for (int32 j = 0; j < SpawnedBlocks.Num(); ++j)
			{
				if (i == j) continue; // 자기 자신 제외
				if (SpawnedBlocks[j] && IsValid(SpawnedBlocks[j]))
				{
					// 이 컴포넌트가 이동할 때 해당 액터를 무시하도록 설정
					RootPrim->IgnoreActorWhenMoving(SpawnedBlocks[j], true);
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

	if (CurrentMovedDistance >= MaxChargeDistance)
	{
		UE_LOG(LogTemp, Log, TEXT("GA_SummonBarrier: Max distance reached. Destroying wall."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 역순 순회 (삭제 대응)
	for (int32 i = SpawnedBlocks.Num() - 1; i >= 0; --i)
	{
		ADestructibleBlock* Block = SpawnedBlocks[i];

		if (!Block || !IsValid(Block))
		{
			SpawnedBlocks.RemoveAt(i);
			continue;
		}

		FHitResult HitResult;
		// Sweep 이동. StartBarrierCharge에서 서로 Ignore 설정을 했으므로
		// 이제 옆 블록에 막히지 않고 전진함
		Block->AddActorWorldOffset(DeltaMove, true, &HitResult);

		if (HitResult.bBlockingHit)
		{
			AActor* HitActor = HitResult.GetActor();

			// Ignore 설정을 했더라도 혹시 모를 안전장치
			if (SpawnedBlocks.Contains(HitActor)) continue;
			if (HitActor == GetAvatarActorFromActorInfo()) continue;

			UE_LOG(LogTemp, Log, TEXT("GA_SummonBarrier: Block %s hit obstacle %s"), *Block->GetName(), *GetNameSafe(HitActor));

			Block->SelfDestroy();
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
		Super::OnCancelPressed(TimeWaited);
	}
	else
	{
		if (!bIsCharging)
		{
			StartBarrierCharge(TimeWaited);
		}
	}
}