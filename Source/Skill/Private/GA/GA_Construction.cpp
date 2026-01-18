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

		if (!BlockSpawner) {
			// BlockSpawner 캐싱
			BlockSpawner = IBlockSpawnInterface::GetBlockManagerSubsystem(World);
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
		AActor* HitActor = HitResult.GetActor();
		IBlockInfoInterface* HitBlockInfo = Cast<IBlockInfoInterface>(HitActor);
		
	// 사거리 내(파란 영역)의 블록인지 확인
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
			// 프리뷰 블록이 없으면 생성 (BP에서 미리 디자인된 프리뷰 블록 사용)
			if (!PreviewBlock.IsValid() && PreviewBlockClass)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				
			AActor* SpawnedPreview = GetWorld()->SpawnActor<AActor>(
					PreviewBlockClass, 
					FVector::ZeroVector, 
					FRotator::ZeroRotator, 
					SpawnParams
				);
				
			if (SpawnedPreview)
			{
				// WeakObjectPtr에 할당
				PreviewBlock = SpawnedPreview;
				// 충돌 비활성화
				SpawnedPreview->SetActorEnableCollision(false);
			}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to spawn PreviewBlock"));
				}
			}

		// 프리뷰 블록을 타겟 블록 위에 배치
		if (PreviewBlock.IsValid())
		{
			AActor* PreviewActor = PreviewBlock.Get();
			FVector BlockLocation = HitBlockInfo->GetBlockLocation();
				FRotator BlockRotation = HitBlockInfo->GetBlockRotation();
				
				// 블록 크기만큼 위로 올림 (블록이 100x100x100이라 가정)
				FVector PreviewLocation = BlockLocation + FVector(0, 0, 100.0f);
				
			FCollisionQueryParams CheckParams;
			CheckParams.AddIgnoredActor(PreviewActor);
			CheckParams.AddIgnoredActor(OwnerPawn); // 플레이어 충돌 제외

				// 블록 크기(50)보다 약간 작게(45) 설정하여 인접 블록과의 미세한 간섭 방지
				bool bIsOccupied = GetWorld()->OverlapBlockingTestByChannel(
					PreviewLocation,
					FQuat::Identity,
					ECC_WorldStatic,
					FCollisionShape::MakeBox(FVector(45.0f)),
					CheckParams
				);

			if (!bIsOccupied)
			{
				// 비어있는 공간이면 프리뷰 표시
				PreviewActor->SetActorLocation(PreviewLocation);
				PreviewActor->SetActorRotation(BlockRotation);
				PreviewActor->SetActorHiddenInGame(false);
			}
			else
			{
				// 이미 자리에 블록이 있으면 숨김 처리
				PreviewActor->SetActorHiddenInGame(true);
			}
			}
		}
	else
	{
		// 마우스 포인터가 가리키는 블록이 범위 밖이면 프리뷰 숨김
		if (PreviewBlock.IsValid())
		{
			PreviewBlock.Get()->SetActorHiddenInGame(true);
		}
	}
}
else
{
	// 마우스 포인터가 가리키는 곳에서 Block 응답을 가진 충돌이 없으면 프리뷰 숨김
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

	AActor* NewBlock = BlockSpawner->SpawnBlockByTag(TAG_Block_Type_Destructible, SpawnLocation, SpawnRotation, true);

	if (NewBlock)
	{
		UE_LOG(LogTemp, Log, TEXT("GA_Construction: Spawned new block %s at location %s"), *NewBlock->GetName(), *SpawnLocation.ToString());
		
		// 스킬 종료
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to spawn block"));
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

