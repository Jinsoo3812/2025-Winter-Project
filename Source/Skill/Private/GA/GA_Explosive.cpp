// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/GA_Explosive.h"
#include "Object/Explosive.h"
#include "Block/BlockBase.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"

UGA_Explosive::UGA_Explosive()
{
}

void UGA_Explosive::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 조준 모드(프리뷰) 진입
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 프리뷰 타이머 시작
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TickTimerHandle, this, &UGA_Explosive::UpdatePreview, 0.016f, true);
	}

	// 프리뷰 태그 부착
	NotifySkillPreviewStarted();

	// 취소 입력 대기 (스킬 재입력시 조준 취소)
	InputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (InputTask)
	{
		InputTask->OnPress.AddDynamic(this, &UGA_Explosive::OnCancelPressed);
		InputTask->ReadyForActivation();
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: Failed to create WaitInputTask for targeting"));
	}

	// 좌클릭 바인딩 (투척 확정)
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			PC->InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &UGA_Explosive::OnLeftClickPressed);
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("GA_Explosive: PlayerController or InputComponent is null"));
		}
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
	ClearHighlights();

	// 폭발물 객체의 착륙 델리게이트 해제
	if (CurrentExplosive && CurrentExplosive->OnLandedDelegate.IsBound())
	{
		CurrentExplosive->OnLandedDelegate.RemoveDynamic(this, &UGA_Explosive::OnExplosiveLanded);
	}

	// 태스크 정리
	if (InputTask)
	{
		InputTask->EndTask();
		InputTask = nullptr;
	}
	if (DelayTask)
	{
		DelayTask->EndTask();
		DelayTask = nullptr;
	}

	// 입력 바인딩 해제
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			for (int32 i = PC->InputComponent->KeyBindings.Num() - 1; i >= 0; --i)
			{
				if (PC->InputComponent->KeyBindings[i].KeyDelegate.GetUObject() == this)
				{
					PC->InputComponent->KeyBindings.RemoveAt(i);
				}
			}
		}
	}

	SavedTargetBlock.Reset();

	NotifySkillPreviewFinished();
	NotifySkillCastFinished();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Explosive::UpdatePreview()
{
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn) return;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) return;

	// 이전 프레임의 하이라이트 초기화
	ClearHighlights();

	// 사거리 내의 모든 블록 탐색 (SkillBase의 함수 활용, RangeXY/RangeZ 사용)
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

	// 현재 마우스가 가리키는 블록이 '사거리 내(파란 영역)'에 있는지 확인하여 타겟팅
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Visibility, true, HitResult);
	ABlockBase* HitBlock = Cast<ABlockBase>(HitResult.GetActor());

	// 마우스 밑의 블록이 사거리(파란 영역) 안에 포함되어 있다면 유효한 타겟(초록색)으로 설정
	if (HitBlock && PreviewedBlocks.Contains(HitBlock))
	{
		// 이 블록만 초록색(3.0)으로 변경
		UStaticMeshComponent* HitMesh = HitBlock->GetBlockMesh();
		if (HitMesh)
		{
			HitMesh->SetCustomPrimitiveDataFloat(0, 3.0f);
		}

		// 투척 대상으로 설정
		HighlightedBlock = HitBlock;
	}
	else
	{
		// 사거리 밖이거나 허공을 가리키면 타겟 없음
		// 이 경우 마우스 오버 효과(초록)는 없고,  파란색만 남음
		HighlightedBlock.Reset();
	}
}

void UGA_Explosive::OnLeftClickPressed()
{
	// 하이라이트된 블록이 유효할 때만 시전
	if (HighlightedBlock.IsValid())
	{
		// 프리뷰 종료
		NotifySkillPreviewFinished();

		// 스킬 시전 시작 (Busy 태그 등 적용)
		NotifySkillCastStarted();

		// 폭발물 투척
		SpawnExplosive();

		// 스킬 시전 종료(현재는 즉발식, 추후 애니메이션 등 추가 시점에 맞춰 조정 필요)
		NotifySkillCastFinished();
	}
}

void UGA_Explosive::OnCancelPressed(float TimeWaited)
{
	// 시전 취소
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UGA_Explosive::SpawnExplosive()
{
	// 커밋 (쿨타임, 비용 지불)
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_Explosive: Failed to commit ability"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	if (!ExplosiveClass)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: ExplosiveClass is not set"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	SavedTargetBlock = HighlightedBlock.Get();

	// 프리뷰 종료: 폭발물이 날아가는 동안엔 빨간색이 꺼져야 함
	// 타이머를 끄고 하이라이트를 제거
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}
	ClearHighlights(); // 이때 HighlightedBlock은 null이 되지만, 위에서 SavedTargetBlock에 백업해둠

	// 2. 좌클릭 바인딩 해제 (더 이상 투척 불가)
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			// 키 바인딩 루프를 돌며 현재 객체에 연결된 바인딩(좌클릭) 제거
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

	// 3. 조준 취소용 입력 태스크 종료
	if (InputTask)
	{
		InputTask->EndTask();
		InputTask = nullptr;
	}

	// 4. 폭발물 생성
	FVector SpawnLoc = OwnerPawn->GetActorLocation();
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AExplosive* NewExplosive = GetWorld()->SpawnActor<AExplosive>(ExplosiveClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);

	if (NewExplosive && SavedTargetBlock.IsValid())
	{
		CurrentExplosive = NewExplosive;

		// 착륙 델리게이트 바인딩
		CurrentExplosive->OnLandedDelegate.AddDynamic(this, &UGA_Explosive::OnExplosiveLanded);

		// 투척 (1.5초 비행)
		CurrentExplosive->Initialize(SpawnLoc, SavedTargetBlock.Get(), 1.5f);

		UE_LOG(LogTemp, Log, TEXT("GA_Explosive: Bomb thrown, waiting for landing..."));

		// EndAbility를 호출하지 않음! (Latent 상태 유지)
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: Failed to spawn bomb or invalid target"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UGA_Explosive::OnExplosiveLanded()
{
	UE_LOG(LogTemp, Log, TEXT("GA_Explosive: Bomb landed. Waiting for manual detonation or timeout."));

	// 착탄한 블록만 빨간색(2.0)으로 다시 켭니다.
	if (SavedTargetBlock.IsValid())
	{
		ABlockBase* LandedBlock = SavedTargetBlock.Get();
		UStaticMeshComponent* Mesh = LandedBlock->GetBlockMesh();
		if (Mesh)
		{
			Mesh->SetCustomPrimitiveDataFloat(0, 2.0f);

			// 나중에 EndAbility에서 ClearHighlights 호출 시 꺼지도록 목록에 추가
			PreviewedBlocks.Add(LandedBlock);
		}
	}

	// 1. 자동 폭파 타이머 태스크 시작
	DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, AutoDetonateDelay);
	if (DelayTask)
	{
		DelayTask->OnFinish.AddDynamic(this, &UGA_Explosive::OnAutoDetonateTimeFinish);
		DelayTask->ReadyForActivation();
	}

	// 2. 수동 기폭 입력 태스크 시작 (스킬 키 재입력)
	InputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (InputTask)
	{
		InputTask->OnPress.AddDynamic(this, &UGA_Explosive::OnManualDetonatePress);
		InputTask->ReadyForActivation();
	}
}

void UGA_Explosive::OnManualDetonatePress(float TimeWaited)
{
	UE_LOG(LogTemp, Log, TEXT("GA_Explosive: Manual detonation triggered."));
	PerformDetonateAndEnd();
}

void UGA_Explosive::OnAutoDetonateTimeFinish()
{
	UE_LOG(LogTemp, Log, TEXT("GA_Explosive: Auto detonation timer finished."));
	PerformDetonateAndEnd();
}

void UGA_Explosive::PerformDetonateAndEnd()
{
	if (CurrentExplosive)
	{
		CurrentExplosive->Detonate();
		CurrentExplosive = nullptr;
	}

	// 모든 작업 완료, 정상 종료
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// 하이라이트 제거 로직
void UGA_Explosive::ClearHighlights()
{
	for (TWeakObjectPtr<ABlockBase>& WeakBlock : PreviewedBlocks)
	{
		if (WeakBlock.IsValid())
		{
			UStaticMeshComponent* MeshComp = WeakBlock->GetBlockMesh();
			if (MeshComp)
			{
				// Index 0 값을 0.0(기본)으로 복구
				MeshComp->SetCustomPrimitiveDataFloat(0, 0.0f);
			}
		}
	}

	// 목록 초기화
	PreviewedBlocks.Empty();
	HighlightedBlock.Reset();
}
