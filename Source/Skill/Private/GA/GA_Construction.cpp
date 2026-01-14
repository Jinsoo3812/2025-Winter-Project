// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/GA_Construction.h"
#include "Block/DestructibleBlock.h"
#include "Block/BlockBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "InputCoreTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Components/InputComponent.h"

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
	}

	// WaitInputPress 어빌리티 태스크 생성
	WaitInputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (WaitInputTask)
	{
		// OnPress 델리게이트에 콜백 함수(스킬 취소) 바인딩
		WaitInputTask->OnPress.AddDynamic(this, &UGA_Construction::OnCancelPressed);

		// 어빌리티 태스크 활성화
		WaitInputTask->ReadyForActivation();
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to create WaitInputTask"));
	}

	// 좌클릭 입력 바인딩
	// 좌클릭은 사용 범위가 넓고, 여러 어빌리티에서 공통으로 사용될 수 있으므로
	// Ability Task 대신 직접 InputComponent에 바인딩
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			// 좌클릭 키 바인딩 추가
			PC->InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &UGA_Construction::OnLeftClickPressed);
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("GA_Construction: PlayerController or InputComponent is null"));
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: OwnerPawn is null"));
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

	// 하이라이트 제거
	ClearHighlights();

	// 프리뷰 블록 제거
	if (PreviewBlock)
	{
		PreviewBlock->Destroy();
		PreviewBlock = nullptr;
	}

	// Ability Task 정리
	if (WaitInputTask)
	{
		WaitInputTask->EndTask();
		WaitInputTask = nullptr;
	}

	// 좌클릭 바인딩 명시적 해제
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			// InputComponent에서 이 객체에 바인딩된 모든 키 바인딩 제거 (ASC를 사용하지 않았기에 수동 제거)
			for (int32 i = PC->InputComponent->KeyBindings.Num() - 1; i >= 0; --i)
			{
				if (PC->InputComponent->KeyBindings[i].KeyDelegate.GetUObject() == this)
				{
					PC->InputComponent->KeyBindings.RemoveAt(i);
				}
			}
		}
	}

	// 끝내는 함수는 자식이 먼저 호출하고, 마지막에 부모 함수 호출
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Construction::HighlightBlocksInRange()
{
	// SkillBase의 FindBlocksInRange를 활용하여 범위 내 블록 탐색
	TArray<ABlockBase*> BlocksInRange;
	FindBlocksInRange(BlocksInRange);

	// 탐색된 블록들에 파란색 하이라이트 적용 (CustomPrimitiveData)
	for (ABlockBase* Block : BlocksInRange)
	{
		if (Block)
		{
			UStaticMeshComponent* Mesh = Block->GetBlockMesh();
			if (Mesh)
			{
				// CustomPrimitiveData Index 0: 1.0f = Preview (Blue)
				Mesh->SetCustomPrimitiveDataFloat(0, 1.0f);
			}
			// 나중에 끄기 위해 목록에 추가
			PreviewedBlocks.Add(Block);
		}
	}
}

void UGA_Construction::ClearHighlights()
{
	for (ABlockBase* Block : PreviewedBlocks)
	{
		if (!Block)
		{
			UStaticMeshComponent* MeshComp = Block->GetBlockMesh();
			if (MeshComp)
			{
				// Index 0 값을 0.0(기본)으로 복구
				MeshComp->SetCustomPrimitiveDataFloat(0, 0.0f);
			}
		}
	}

	// 목록 초기화
	PreviewedBlocks.Empty();
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

	// 이전 프레임의 하이라이트 초기화
	ClearHighlights();

	// 범위 내 블록들을 찾아서 파란색 하이라이트
	HighlightBlocksInRange();

	// 마우스 커서 아래 블록 찾기
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Visibility, true, HitResult);

	// bBlockingHit은 Block 응답을 가진 충돌이 발생했는지 여부
	if (HitResult.bBlockingHit)
	{
		ABlockBase* HitBlock = Cast<ABlockBase>(HitResult.GetActor());
		
		// 사거리 내(파란 영역)의 블록인지 확인
		if (HitBlock && PreviewedBlocks.Contains(HitBlock))
		{
			// 프리뷰 블록이 없으면 생성 (BP에서 미리 디자인된 프리뷰 블록 사용)
			if (!PreviewBlock && PreviewBlockClass)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				
				PreviewBlock = GetWorld()->SpawnActor<AActor>(
					PreviewBlockClass, 
					FVector::ZeroVector, 
					FRotator::ZeroRotator, 
					SpawnParams
				);
				
				if (PreviewBlock)
				{
					// 충돌 비활성화
					PreviewBlock->SetActorEnableCollision(false);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to spawn PreviewBlock"));
				}
			}

			// 프리뷰 블록을 타겟 블록 위에 배치
			if (PreviewBlock)
			{
				FVector BlockLocation = HitBlock->GetActorLocation();
				FRotator BlockRotation = HitBlock->GetActorRotation();
				
				// 블록 크기만큼 위로 올림 (블록이 100x100x100이라 가정)
				FVector PreviewLocation = BlockLocation + FVector(0, 0, 100.0f);
				
				FCollisionQueryParams CheckParams;
				CheckParams.AddIgnoredActor(PreviewBlock);
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
					PreviewBlock->SetActorLocation(PreviewLocation);
					PreviewBlock->SetActorRotation(BlockRotation);
					PreviewBlock->SetActorHiddenInGame(false);
				}
				else
				{
					// 이미 자리에 블록이 있으면 숨김 처리
					PreviewBlock->SetActorHiddenInGame(true);
				}
			}
		}
		else
		{
			// 마우스 포인터가 가리키는 블록이 범위 밖이면 프리뷰 숨김
			if (PreviewBlock)
			{
				PreviewBlock->SetActorHiddenInGame(true);
			}
		}
	}
	else
	{
		// 마우스 포인터가 가리키는 곳에서 Block 응답을 가진 충돌이 없으면 프리뷰 숨김
		if (PreviewBlock)
		{
			PreviewBlock->SetActorHiddenInGame(true);
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

	if (!PreviewBlock || PreviewBlock->IsHidden()) return;

	if (!BlockToSpawn)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: BlockToSpawn is null in SpawnBlock"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// 프리뷰 블록 위치에 실제 블록 생성
	FVector SpawnLocation = PreviewBlock->GetActorLocation();
	FRotator SpawnRotation = PreviewBlock->GetActorRotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADestructibleBlock* NewBlock = World->SpawnActor<ADestructibleBlock>(BlockToSpawn, SpawnLocation, SpawnRotation, SpawnParams);

	if (NewBlock)
	{
		// 블록 초기화 (필요한 경우)
		NewBlock->SpawnBlock(SpawnLocation, EBlockType::Destructible);
		
		UE_LOG(LogTemp, Log, TEXT("GA_Construction: Spawned new block %s at location %s"), *NewBlock->GetName(), *SpawnLocation.ToString());
		
		// 스킬 종료
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to spawn block"));
	}
}

void UGA_Construction::OnLeftClickPressed()
{
	// 프리뷰 블록이 존재하고, 숨겨져 있지 않을 때만 블록 생성 시도
	if (PreviewBlock && !PreviewBlock->IsHidden())
	{
		// 실제 스킬 시전 시작 알림
		// State.Busy 태그를 부여
		NotifySkillCastStarted();
		// 좌클릭 시 블록 생성 시도
		SpawnBlock();
	}
}

void UGA_Construction::OnCancelPressed(float TimeWaited)
{
	// W키 재입력 시 스킬 취소
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

