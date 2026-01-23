// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/GA_Construction.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/InputComponent.h"
#include "InputGameplayTags.h"
#include "BlockGameplayTags.h"
#include "BlockInfoInterface.h"
#include "BlockSpawnInterface.h"
#include "CollisionChannels.h"
#include "BlockSystemInterface.h"
#include "BlockCommon.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

UGA_Construction::UGA_Construction() {}

void UGA_Construction::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 프리뷰 업데이트를 위한 타이머 시작 (매 프레임)
	if (UWorld* World = GetWorld())
	{	
		// 60FPS 간격으로 UpdatePreview 함수 호출
		// 자식이 재정의한 UpdatePreview 또한 호출될 수 있음.
		World->GetTimerManager().SetTimer(TickTimerHandle, this, &UGA_Construction::UpdatePreview, 0.016f, true);

		if (!BlockSystem) {
			BlockSystem = IBlockSystemInterface::Get(World);
		}
	}

	// WaitInputPress 어빌리티 태스크 생성
	WaitInputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (WaitInputTask)
	{
		WaitInputTask->OnPress.AddDynamic(this, &UGA_Construction::OnCancelPressed);
		WaitInputTask->ReadyForActivation();
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to create WaitInputTask"));
	}

	UAbilityTask_WaitGameplayEvent* WaitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		TAG_Input_LeftClick,
		nullptr,
		false,
		false
	);

	if (WaitEventTask)
	{
		WaitEventTask->EventReceived.AddDynamic(this, &UGA_Construction::OnLeftClickEventReceived);
		WaitEventTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to create WaitGameplayEvent task"));
		// 태스크 생성 실패 시 안전하게 종료하거나 예외 처리
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGA_Construction::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}

	// 타이머 핸들 무효화
	TickTimerHandle.Invalidate();

	// 바닥 프리뷰 하이라이트 제거
	ClearHighlights(PreviewBlocks);

	// 프리뷰 블록 제거
	if (PreviewBlock.IsValid())
	{
		PreviewBlock.Get()->Destroy();
		PreviewBlock.Reset();
	}

	// Ability Task 정리
	if (WaitInputTask)
	{
		WaitInputTask->EndTask();
		WaitInputTask = nullptr;
	}

	// 끝내는 함수는 자식이 먼저 호출하고, 마지막에 부모 함수 호출
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Construction::UpdatePreview()
{
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: OwnerPawn is null in UpdatePreview"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: PlayerController is null in UpdatePreview"));
		return;
	}

	// 범위 내 블록들을 찾아서 파란색 하이라이트
	HighlightBlocks(PreviewBlocks, TAG_Block_Highlight_Preview);

	// 마우스 커서 아래 블록 찾기
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Block, true, HitResult);

	// bBlockingHit은 Block 응답을 가진 충돌이 발생했는지 여부
	if (HitResult.bBlockingHit)
	{
		// [핵심 변경] HitResult를 분석하여 FBlockReference 생성
		FBlockReference HitBlockRef;

		// Case A: HISM (Terrain)
		if (UHierarchicalInstancedStaticMeshComponent* HISM = Cast<UHierarchicalInstancedStaticMeshComponent>(HitResult.GetComponent()))
		{
			HitBlockRef.TargetObject = HitResult.GetActor();
			HitBlockRef.TargetComponent = HISM;
			HitBlockRef.ItemIndex = HitResult.Item;
		}
		// Case B: Actor (설치된 블록)
		else if (AActor* HitActor = HitResult.GetActor())
		{
			HitBlockRef.TargetObject = HitActor;
			HitBlockRef.TargetComponent = HitResult.GetComponent();
			HitBlockRef.ItemIndex = -1;
		}

		// 3. 유효한 블록인지 확인 (범위 내에 있는지)
		float DistSq = FVector::DistSquared(OwnerPawn->GetActorLocation(), HitResult.ImpactPoint);
		float RangeSq = RangeXY * RangeXY;

		// 범위 안이고, 유효한 블록(Terrain or Actor)일 때
		if (HitBlockRef.IsValid() && DistSq <= RangeSq)
		{
			// 4. 시스템에게 "이 블록의 정렬된 위치(Center)가 어디냐"고 물어봄
			FVector BlockLocation = BlockSystem->GetBlockLocation(HitBlockRef);
			float GridSize = BlockSystem->GetGridSize(); // 100.0f

			// 프리뷰 액터 스폰 (없으면)
			if (!PreviewBlock.IsValid() && PreviewBlockClass)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				AActor* SpawnedPreview = GetWorld()->SpawnActor<AActor>(PreviewBlockClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
				if (SpawnedPreview)
				{
					PreviewBlock = SpawnedPreview;
					SpawnedPreview->SetActorEnableCollision(false);
				}
			}

			// 프리뷰 위치 갱신
			if (PreviewBlock.IsValid())
			{
				AActor* PreviewActor = PreviewBlock.Get();

				// 클릭한 블록의 위쪽으로 위치 설정 (Z + GridSize)
				FVector PreviewLocation = BlockLocation + FVector(0, 0, GridSize);

				// 겹침 검사 (건설 가능 여부)
				// 여기서 IsLocationOccupied는 시스템 인터페이스를 통해 호출
				bool bIsOccupied = BlockSystem->IsLocationOccupied(PreviewLocation, GridSize);

				if (!bIsOccupied)
				{
					PreviewActor->SetActorLocation(PreviewLocation);
					PreviewActor->SetActorRotation(FRotator::ZeroRotator);
					PreviewActor->SetActorHiddenInGame(false);
				}
				else
				{
					// 이미 뭔가 있어서 건설 불가
					PreviewActor->SetActorHiddenInGame(true);
				}
			}
		}
		else
		{
			// 범위 밖
			if (PreviewBlock.IsValid())
			{
				PreviewBlock.Get()->SetActorHiddenInGame(true);
			}
		}
	}
	else
	{
		// 허공
		if (PreviewBlock.IsValid())
		{
			PreviewBlock.Get()->SetActorHiddenInGame(true);
		}
	}
}

void UGA_Construction::SpawnBlock()
{
	// Ability 활성화 커밋 (Cost, Cooldown 등 체크 및 적용)

	if (!CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to commit ability"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	if (!PreviewBlock.IsValid() || PreviewBlock.Get()->IsHidden()) return; 

	// 프리뷰 블록 위치에 실제 블록 생성
	FVector SpawnLocation = PreviewBlock.Get()->GetActorLocation();
	FRotator SpawnRotation = PreviewBlock.Get()->GetActorRotation();

	// [수정] BlockSystem을 통해 생성 요청
	if (BlockSystem)
	{
		AActor* NewBlock = BlockSystem->SpawnBlockByTag(TAG_Block_Type_Destructible, SpawnLocation, SpawnRotation, true);

		if (NewBlock)
		{
			// 성공 시 스킬 종료
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}
}

void UGA_Construction::OnCancelPressed(float TimeWaited)
{
	// W키 재입력 시 스킬 취소
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UGA_Construction::OnLeftClickEventReceived(FGameplayEventData Payload)
{
	// 프리뷰 블록이 존재하고, 숨겨져 있지 않을 때만 블록 생성 시도
	if (PreviewBlock.IsValid() && !PreviewBlock.Get()->IsHidden())
	{
		// 실제 스킬 시전 시작 알림
		NotifySkillCastStarted();
		// 블록 생성 시도
		SpawnBlock();
	}
	else
	{
		// 프리뷰가 유효하지 않을 때 클릭하면 로그 (디버깅용)
		UE_LOG(LogTemp, Verbose, TEXT("GA_Construction: Clicked but invalid preview"));
	}
}

