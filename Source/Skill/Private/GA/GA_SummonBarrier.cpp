// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/GA_SummonBarrier.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/PrimitiveComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

// [필수] 시스템 인터페이스 및 태그
#include "BlockSystemInterface.h"
#include "BlockGameplayTags.h"
#include "InputGameplayTags.h"
#include "Collision/CollisionChannels.h"
#include "AbilitySystemComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

UGA_SummonBarrier::UGA_SummonBarrier()
{
	// 기본값 초기화
	CurrentMovedDistance = 0.0f;
	bIsCharging = false;
	ChargeDirection = FVector::ForwardVector;
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

	// [부모 기능] 바닥 타일 하이라이트 정리
	ClearHighlights(PreviewBlockRefs);

	// 스킬이 취소/종료되면 남은 실제 블록들도 모두 제거 (돌진 완료 전 종료 시)
	for (TWeakObjectPtr<AActor>& Block : SpawnedBlocks)
	{
		if (Block.IsValid())
		{
			Block.Get()->Destroy();
		}
	}
	SpawnedBlocks.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_SummonBarrier::UpdatePreview()
{
	// 건설이 이미 완료되어 대기/돌진 중이라면 프리뷰를 업데이트하지 않음
	if (SpawnedBlocks.Num() > 0) return;

	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn) return;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) return;

	// [1] 범위 내 블록 하이라이트 (GA_SkillBase 기능 활용)
	// PreviewBlockRefs 배열에 범위 내 블록(HISM 포함)들이 담김
	HighlightBlocks(PreviewBlockRefs, TAG_Block_Highlight_Preview);

	// [2] 마우스 커서 위치 계산
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Block, true, HitResult);

	bool bValidTargetFound = false;
	TArray<FTransform> TargetTransforms;

	if (HitResult.bBlockingHit && BlockSystem)
	{
		// HitResult를 FBlockReference로 변환 (HISM 인덱스 지원)
		FBlockReference HitRef;

		// Case A: HISM (Terrain / Chunk)
		if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(HitResult.GetComponent()))
		{
			HitRef.TargetObject = HitResult.GetActor();
			HitRef.TargetComponent = HISM;
			HitRef.ItemIndex = HitResult.Item;
		}
		// Case B: Actor (설치된 블록)
		else if (AActor* HitActor = HitResult.GetActor())
		{
			HitRef.TargetObject = HitActor;
			HitRef.TargetComponent = HitResult.GetComponent();
			HitRef.ItemIndex = -1;
		}

		// [3] 클릭한 블록이 사거리(파란색 하이라이트) 안에 있는지 검사
		bool bIsInRange = false;
		for (const FBlockReference& Ref : PreviewBlockRefs)
		{
			// FBlockReference는 구조체이므로 멤버별 비교 필요 (또는 연산자 오버로딩)
			if (Ref.TargetObject == HitRef.TargetObject &&
				Ref.TargetComponent == HitRef.TargetComponent &&
				Ref.ItemIndex == HitRef.ItemIndex)
			{
				bIsInRange = true;
				break;
			}
		}

		if (bIsInRange)
		{
			bValidTargetFound = true;

			// 시스템 설정 가져오기
			GridSize = BlockSystem->GetGridSize();
			float HalfGridSize = GridSize * 0.5f;

			// [중요] 시스템을 통해 블록의 '정확한 그리드 중심' 위치 획득
			FVector BlockLocation = BlockSystem->GetBlockLocation(HitRef);

			// 바닥 블록의 윗면 중심점
			FVector CenterBaseLocation = BlockLocation + FVector(0, 0, GridSize); // 혹은 HalfGridSize 등 오프셋 조정
			FVector CurrentPlayerLocation = OwnerPawn->GetActorLocation();

			// 방벽 3x2 모양 계산
			CalculateBarrierTransforms(CenterBaseLocation, CurrentPlayerLocation, TargetTransforms);
		}
	}

	// [4] 결과 반영
	if (bValidTargetFound)
	{
		UpdateBarrierPreviewActors(TargetTransforms);
	}
	else
	{
		// 타겟이 없으면 모든 프리뷰 숨김
		for (auto& Preview : BarrierPreviewBlocks)
		{
			if (Preview.IsValid()) Preview.Get()->SetActorHiddenInGame(true);
		}
	}
}

void UGA_SummonBarrier::CalculateBarrierTransforms(const FVector& CenterLocation, const FVector& PlayerLocation, TArray<FTransform>& OutTransforms)
{
	// Player에서 Center로 향하는 방향 벡터 계산 (방벽의 회전 결정)
	FVector Direction = CenterLocation - PlayerLocation;
	FRotator WallRotation;
	FVector RightVector;

	// 방향 결정 로직 (간소화됨: X축 혹은 Y축 정렬)
	if (FMath::Abs(Direction.X) > FMath::Abs(Direction.Y))
	{
		WallRotation = FRotator(0, 90.0f, 0);
		RightVector = FVector(0, 1.0f, 0); // Y축 방향으로 뻗는 방벽
	}
	else
	{
		WallRotation = FRotator::ZeroRotator;
		RightVector = FVector(1.0f, 0, 0); // X축 방향으로 뻗는 방벽
	}

	OutTransforms.Empty();

	// 좌우 대칭 3칸: {-1, 0, 1}
	float Offsets[] = { 0.0f, -1.0f, 1.0f };

	for (float OffsetMultiplier : Offsets)
	{
		// 1층 기준점
		FVector BasePos = CenterLocation + (RightVector * (GridSize * OffsetMultiplier));

		// 1층 추가
		OutTransforms.Add(FTransform(WallRotation, BasePos));

		// 2층 추가 (GridSize 만큼 위로)
		FVector UpperPos = BasePos + FVector(0, 0, GridSize);
		OutTransforms.Add(FTransform(WallRotation, UpperPos));
	}
}

void UGA_SummonBarrier::UpdateBarrierPreviewActors(const TArray<FTransform>& Transforms)
{
	// [크래시 방지] World 확인
	UWorld* World = GetWorld();
	if (!World) return;

	if (!PreviewBlockClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_SummonBarrier: PreviewBlockClass is not set."));
		return;
	}

	// [크래시 해결] BlockSystem 유효성 검사 (기존 BlockSpawner -> BlockSystem)
	if (!BlockSystem)
	{
		// 시스템 로드 시도
		BlockSystem = IBlockSystemInterface::Get(World);
		if (!BlockSystem) return;
	}

	// 프리뷰 액터 풀링 (부족하면 생성)
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

		// 이번 프레임에 사용할 프리뷰라면
		if (i < Transforms.Num())
		{
			const FTransform& TargetTransform = Transforms[i];

			// [수정] 시스템을 통해 해당 위치가 비어있는지 확인
			bool bOccupied = BlockSystem->IsLocationOccupied(TargetTransform.GetLocation(), GridSize);

			if (bOccupied)
			{
				// 소환 불가: 숨김 (또는 붉은색 표시)
				Preview->SetActorHiddenInGame(true);
			}
			else
			{
				// 소환 가능: 위치 이동 및 보이기
				Preview->SetActorTransform(TargetTransform);
				Preview->SetActorHiddenInGame(false);
			}
		}
		else
		{
			// 남는 프리뷰는 숨김
			Preview->SetActorHiddenInGame(true);
		}
	}
}

void UGA_SummonBarrier::SpawnBlock()
{
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	// [수정] BlockSystem 유효성 검사 (BlockSpawner -> BlockSystem)
	if (!BlockSystem)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_SummonBarrier: BlockSystem is missing!"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());

	SpawnedBlocks.Empty();
	FVector AverageLocation = FVector::ZeroVector;
	int32 Count = 0;

	// 프리뷰 액터 위치에 실제 블록 생성
	for (TWeakObjectPtr<AActor>& Preview : BarrierPreviewBlocks)
	{
		if (!Preview.IsValid() || Preview.Get()->IsHidden()) continue;

		FVector SpawnLoc = Preview.Get()->GetActorLocation();
		FRotator SpawnRot = Preview.Get()->GetActorRotation();

		// [수정] SpawnBlockByTag 사용. bEnableGravity=false로 설정하여 즉시 떨어지지 않게 함
		AActor* NewBlock = BlockSystem->SpawnBlockByTag(TAG_Block_Type_Destructible, SpawnLoc, SpawnRot, false);

		if (NewBlock)
		{
			// 위치 재보정 (미세 오차 방지)
			NewBlock->SetActorLocationAndRotation(SpawnLoc, SpawnRot);

			// Tick 비활성화 (Grid Snap 방지용)
			NewBlock->SetActorTickEnabled(false);

			// 방벽 이동을 위해 Movable로 설정 (기본이 Static/Stationary 일 수 있음)
			if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(NewBlock->GetRootComponent()))
			{
				RootPrim->SetMobility(EComponentMobility::Movable);
			}

			SpawnedBlocks.Add(NewBlock);
			AverageLocation += SpawnLoc;
			Count++;
		}
	}

	// 프리뷰 제거 및 정리
	for (auto& Preview : BarrierPreviewBlocks)
	{
		if (Preview.IsValid()) Preview.Get()->Destroy();
	}
	BarrierPreviewBlocks.Empty();
	ClearHighlights(PreviewBlockRefs);

	if (Count == 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 1단계(건설) 완료 알림
	NotifySkillCastFinished();

	// 돌진 방향 계산
	if (SpawnedBlocks.Num() > 0 && OwnerPawn)
	{
		AverageLocation /= Count;
		FVector PlayerLoc = OwnerPawn->GetActorLocation();

		// 건설 당시 위치 기준으로 돌진 방향 고정
		FVector DirectionToWall = (AverageLocation - PlayerLoc).GetSafeNormal2D();

		if (DirectionToWall.IsZero())
		{
			DirectionToWall = OwnerPawn->GetActorForwardVector().GetSafeNormal2D();
		}

		// 축 정렬 (X or Y)
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

	// 돌진 입력을 위한 태스크 시작
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
}

void UGA_SummonBarrier::StartBarrierCharge(float TimeWaited)
{
	if (bIsCharging) return;

	bIsCharging = true;
	CurrentMovedDistance = 0.0f;

	// 이동 시작 전 물리 처리
	for (int32 i = 0; i < SpawnedBlocks.Num(); ++i)
	{
		if (!SpawnedBlocks[i].IsValid()) continue;
		AActor* MyBlock = SpawnedBlocks[i].Get();

		if(BlockSystem)
		{
			// 시스템에 블록 점유 해제 알림
			BlockSystem->DestroyBlocksInRadius(MyBlock->GetActorLocation(), 1.0f);
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("GA_SummonBarrier: BlockSystem is missing during charge start!"));
			return;
		}

		// 바닥 끌림 방지를 위해 살짝 띄움
		MyBlock->AddActorWorldOffset(FVector(0, 0, 0.0f), false);

		// 자기들끼리 충돌 무시 (뭉쳐서 이동하므로)
		if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(MyBlock->GetRootComponent()))
		{
			for (int32 j = 0; j < SpawnedBlocks.Num(); ++j)
			{
				if (i == j) continue;
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

	// 이동 타이머 시작
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ChargeTimerHandle, this, &UGA_SummonBarrier::TickBarrierCharge, 0.016f, true);
	}
}

void UGA_SummonBarrier::TickBarrierCharge()
{
	if (SpawnedBlocks.IsEmpty())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	float DeltaTime = 0.016f;
	float MoveDist = ChargeSpeed * DeltaTime;
	FVector DeltaMove = ChargeDirection * MoveDist;

	CurrentMovedDistance += MoveDist;

	// 최대 거리 도달 체크
	if (CurrentMovedDistance >= MaxChargeDistance)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 역순 순회 (삭제 안전)
	for (int32 i = SpawnedBlocks.Num() - 1; i >= 0; --i)
	{
		AActor* Block = SpawnedBlocks[i].Get();
		if (!Block)
		{
			SpawnedBlocks.RemoveAt(i);
			continue;
		}

		FHitResult HitResult;
		// Sweep 이동으로 충돌 감지
		Block->AddActorWorldOffset(DeltaMove, true, &HitResult);

		if (HitResult.bBlockingHit)
		{
			AActor* HitActor = HitResult.GetActor();

			// [예외 처리]
			// 1. 자기 자신 그룹(동료 방벽)과의 충돌 무시
			// 2. 시전자(플레이어)와의 충돌 무시
			bool bIsSelfGroup = false;
			for (const TWeakObjectPtr<AActor>& SpawnedBlock : SpawnedBlocks)
			{
				if (SpawnedBlock.IsValid() && SpawnedBlock.Get() == HitActor)
				{
					bIsSelfGroup = true;
					break;
				}
			}
			if (bIsSelfGroup) continue;
			if (HitActor == GetAvatarActorFromActorInfo()) continue;

			// 적중 시 데미지 적용
			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
			if (TargetASC)
			{
				// GA_SkillBase의 룬 데미지 스펙 생성 함수 사용
				FGameplayEffectSpecHandle SpecHandle = MakeRuneDamageEffectSpec(CurrentSpecHandle, CurrentActorInfo);
				if (SpecHandle.IsValid())
				{
					GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
						*SpecHandle.Data.Get(),
						TargetASC
					);
				}
			}

			// 충돌한 방벽 블록 파괴
			Block->Destroy();
			SpawnedBlocks.RemoveAt(i);
		}
	}

	// 모든 방벽이 파괴되었다면 스킬 종료
	if (SpawnedBlocks.IsEmpty())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_SummonBarrier::OnCancelPressed(float TimeWaited)
{
	// 방벽이 아직 없으면 -> 일반 취소
	if (SpawnedBlocks.Num() == 0)
	{
		Super::OnCancelPressed(TimeWaited);
	}
	// 방벽이 있으면 -> 돌진 명령
	else
	{
		if (!bIsCharging)
		{
			StartBarrierCharge(TimeWaited);
		}
	}
}

bool UGA_SummonBarrier::CanBeCanceled() const
{
	// 방벽이 생성된 상태라면 취소 불가 (돌진으로 이어져야 함)
	if (SpawnedBlocks.Num() > 0)
	{
		return false;
	}
	return Super::CanBeCanceled();
}

void UGA_SummonBarrier::OnLeftClickEventReceived(FGameplayEventData Payload)
{
	// 프리뷰가 하나라도 보이면 설치 가능
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
		NotifySkillCastStarted();
		SpawnBlock();
	}
}