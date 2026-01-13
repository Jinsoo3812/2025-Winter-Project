// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/GA_Explosive.h"
#include "Object/Explosive.h"
#include "Block/BlockBase.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

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

	// 이미 던져놓은 폭발물이 아직 유효하다면 기폭 및 종료
	if (CurrentExplosive.IsValid())
	{
		// 착륙(Attached) 상태일 때만 기폭 가능
		if (CurrentExplosive->IsAttached())
		{
			PerformDetonateAndEnd();
			return;
		}
		else
		{
			// 아직 날아가는 중이면 기폭하지 않고, 조준 모드 진입도 막기 위해 종료
			UE_LOG(LogTemp, Warning, TEXT("GA_Explosive: Bomb is flying. Cannot detonate yet."));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return;
		}
	}

	// 프리뷰 타이머 시작
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TickTimerHandle, this, &UGA_Explosive::UpdatePreview, 0.016f, true);
	}

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

	// 태스크 정리
	if (InputTask)
	{
		InputTask->EndTask();
		InputTask = nullptr;
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

	// 폭발물에게 정보를 넘겨줄 것이므로 리셋해도 됨
	SavedTargetBlock.Reset();

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
		// 스킬 시전 시작 (Busy 태그 등 적용)
		NotifySkillCastStarted();

		// 폭발물 투척 및 스킬 종료
		SpawnExplosive();
	}
}

void UGA_Explosive::OnCancelPressed(float TimeWaited)
{
	// 조준 취소 및 종료
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UGA_Explosive::SpawnExplosive()
{
	// 투척 시점에는 쿨타임이 적용되지 않음
	if (!CommitAbilityCost(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
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

	// 이제 목표 블록이 바뀌지 않으므로 SavedTargetBlock에 고정 백업
	SavedTargetBlock = HighlightedBlock.Get();

	// 프리뷰 종료: 폭발물이 날아가는 동안엔 빨간색이 꺼져야 함
	// 타이머를 끄고 하이라이트를 제거
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}
	ClearHighlights(); // 이때 HighlightedBlock은 null이 되지만, 위에서 SavedTargetBlock에 백업해둠

	// 좌클릭 바인딩 해제 (더 이상 투척 불가)
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

		NewExplosive->OnDetonatedDelegate.AddDynamic(this, &UGA_Explosive::OnExplosiveDetonated);

		// GA가 종료된 후에도 액터가 데미지를 줄 수 있도록 SpecHandle을 생성하여 전달
		FGameplayEffectSpecHandle DamageSpecHandle = MakeRuneDamageEffectSpec(CurrentSpecHandle, CurrentActorInfo);

		// 폭발물 초기화
		NewExplosive->Initialize(
			SpawnLoc,
			SavedTargetBlock.Get(),
			1.5f,
			AutoDetonateDelay,
			ExplosionRadius,
			GetAbilitySystemComponentFromActorInfo(),
			DamageSpecHandle,
			DestructionEffect
		);

		// 성공적으로 던졌으므로 bWasCancelled = false.
		// 폭발 대기는 이제 액터가 스스로 하거나, 플레이어가 다시 스킬을 눌러 처리
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_Explosive: Failed to spawn bomb or invalid target"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UGA_Explosive::PerformDetonateAndEnd()
{
	if (CurrentExplosive.IsValid())
	{
		// 폭발물 폭파. 피해 및 파괴 처리 로직은 폭발물 액터 내부에서 처리됨
		// Detonate() 호출 시 OnDetonatedDelegate가 브로드캐스트되어 OnExplosiveDetonated()가 실행됨
		CurrentExplosive->Detonate();

		CurrentExplosive.Reset();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_Explosive: Detonate requested but bomb is gone (Already auto-detonated?)."));
	}

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

void UGA_Explosive::OnExplosiveDetonated()
{
	// 수동 OR 자동 폭파 모두 이 함수가 호출되므로 여기서 쿨타임 적용
	if (!CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true))
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_Explosive: Failed to commit cooldown in Delegate."));
	}

	// 폭발물이 터졌으므로 포인터 안전하게 초기화
	if (CurrentExplosive.IsValid())
	{
		CurrentExplosive.Reset();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_Explosive: Received detonation signal, but CurrentExplosive was already invalid."));
	}
}
