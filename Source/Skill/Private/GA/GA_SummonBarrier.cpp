// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/GA_SummonBarrier.h"
#include "Block/DestructibleBlock.h"
#include "Block/BlockBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"

UGA_SummonBarrier::UGA_SummonBarrier()
{
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
	GridSize = BlockToSpawn ? BlockToSpawn->GetDefaultObject<ADestructibleBlock>()->GetGridSize() : 100.0f;
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

	// 바닥 타일 하이라이트 정리
	ClearHighlights();

	// 스킬이 취소/종료되면 남은 블록들도 모두 제거
	for (TObjectPtr<ADestructibleBlock>& Block : SpawnedBlocks)
	{
		if (Block && IsValid(Block))
		{
			Block->Destroy();
		}
	}

	SpawnedBlocks.Empty();

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

	// 1. 이전 프레임의 하이라이트 초기화
    ClearHighlights();

    // 2. 사거리 내 블록 탐색 (부모 클래스 함수 활용)
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

    // 5. 마우스 커서 타겟팅 및 방벽 프리뷰 계산
    FHitResult HitResult;
    PC->GetHitResultUnderCursor(ECC_Visibility, true, HitResult);

    bool bValidTargetFound = false;
    TArray<FTransform> TargetTransforms;

    if (HitResult.bBlockingHit)
    {
        ABlockBase* HitBlock = Cast<ABlockBase>(HitResult.GetActor());

        // 마우스 밑의 블록이 사거리(파란 영역) 안에 포함되어 있을 때만 설치 가능
        if (HitBlock && PreviewedBlocks.Contains(HitBlock))
        {
            bValidTargetFound = true;

            // 방벽의 중심 블록의 생성 위치 계산
            // 중심 블록은 마우스를 가져간 블록의 바로 윗 블록
            FVector CenterBaseLocation = HitBlock->GetActorLocation() + FVector(0, 0, GridSize);
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
			if (Preview) Preview->SetActorHiddenInGame(true);
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
	* 
	*    [ -1 ] [ 0 ] [ 1 ]   <-- 2층 (UpperPos: BasePos + Z축 높이)
	*    [ -1 ] [ 0 ] [ 1 ]   <-- 1층 (BasePos: 기준 바닥 위치)
	*      ↑      ↑      ↑
	*     Left  Center Right
	* 
	* - 0.0f : 중앙 (Center, 마우스가 가리키는 기준점)
	* - -1.0f: 왼쪽 (Left, RightVector 반대 방향으로 1칸)
	* - 1.0f : 오른쪽 (Right, RightVector 방향으로 1칸)
	*/
	float Offsets[] = { 0.0f, -1.0f, 1.0f };

	// 첫 순회에서 가운데 블록, 두 번째 순회에서 왼쪽 블록, 세 번째 순회에서 오른쪽 블록 생성
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
		AActor* Preview = BarrierPreviewBlocks[i];
		if (!Preview) continue;

		// 만들어 놓은 프리뷰 블록(BarrierPreviewBlocks)이
		// 소환 할 블록 개수(Transforms.Num())보다 많다면 숨김 처리
		if (i < Transforms.Num())
		{
			const FTransform& TargetTransform = Transforms[i];
			if (IsLocationOccupied(TargetTransform.GetLocation()))
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

bool UGA_SummonBarrier::IsLocationOccupied(const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_SummonBarrier: World is null in IsLocationOccupied"));
		return true;
	}

	// MakeBox는 인자를 반지름으로 사용함
	// 0.5를 넣으면 100 * 100 * 100 크기의 박스가 되어 꽉 차므로 0.4 사용
	FVector BoxExtent = FVector(GridSize * 0.4f, GridSize * 0.4f, GridSize * 0.4f);
	FCollisionShape CheckShape = FCollisionShape::MakeBox(BoxExtent);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams;
	for (const auto& Preview : BarrierPreviewBlocks)
	{
		// 프리뷰 블록끼리는 충돌 무시
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

	// 좌클릭 바인딩'만' 해제
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

			// 방벽을 구성하는 파괴가능블록은 추락 옵션 비활성화
			NewBlock->SetCanFall(false);

			// StaticMeshComponent는 기본적으로 런타임에 움직일 수 없음
			// 따라서 Mobility를 Movable로 설정
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

	// 소환된 블록이 없으면 종료
	if (Count == 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 방벽 건설이 완료되었으므로, 다른 스킬을 쓸 수 있도록 State.Busy 태그 즉시 제거
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		// GA_SkillBase에 선언된 TAG_Skill_Casting (State.Busy) 사용
		NotifySkillCastFinished();
	}

	// 2. 돌진 방향 설정
	if (SpawnedBlocks.Num() > 0 && SpawnedBlocks[0])
	{
		FVector PlayerLoc = OwnerPawn->GetActorLocation();
		AverageLocation /= Count;
		// 건설 당시 플레이어 위치를 기준으로 발사 방향	결정
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

	// 기존 태스크를 종료시키지 않고 새로운 태스크를 만들면
	// 이전 태스크가 남아있게 되어 다중 바인딩 문제가 발생할 수 있음
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

		// 형제 블록 무시 설정
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

			// 부딪힌 액터가 자기 자신이거나 동료 방벽 블록이면 무시
			if (SpawnedBlocks.Contains(HitActor)) continue;
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
	// 방벽이 이미 생성되어 관리 목록(SpawnedBlocks)에 있다면?
	// 대기 중이거나 돌진 중인 상태이므로 취소되면 안 됨.
	if (SpawnedBlocks.Num() > 0)
	{
		return false;
	}

	// 방벽이 없다면?
	// 아직 건설 전(프리뷰) 상태이므로, 다른 스킬 입력 시 취소(교체)되어야 함.
	return Super::CanBeCanceled();
}

void UGA_SummonBarrier::OnLeftClickPressed()
{
	// BarrierPreviewBlocks 배열 중 하나라도 유효하고(보이고) 있다면 설치 가능한 상태로 간주
	bool bCanSpawn = false;

	for (const auto& Preview : BarrierPreviewBlocks)
	{
		// IsHidden()이 false여야 화면에 보이고 있다는 뜻 
		if (Preview && !Preview->IsHidden())
		{
			bCanSpawn = true;
			break;
		}
	}

	if (bCanSpawn)
	{
		// 실제 스킬 시전 시작 알림 (State.Busy 태그 부여)
		NotifySkillCastStarted();
		// 블록 생성 시도
		SpawnBlock();
	}
}

void UGA_SummonBarrier::ClearHighlights()
{
	// 1. 현재 저장된 블록들의 상태를 'None'으로 복구
	// 배열을 비우기 전에 반드시 실행해야 색상이 돌아옵니다.
	TArray<AActor*> ActorsToReset;
	for (ABlockBase* Block : PreviewedBlocks)
	{
		if (Block)
		{
			ActorsToReset.Add(Block);
		}
	}
	BatchHighlightBlocks(ActorsToReset, 0.0f);

	// 2. 목록 초기화
	PreviewedBlocks.Empty();
}
