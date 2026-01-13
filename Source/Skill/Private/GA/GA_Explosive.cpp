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
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 유효하지 않은(이미 터지거나 파괴된) 폭탄만 찾아 배열에서 정리
	// RemoveAll: 괄호 안의 조건을 만족하는 모든 요소를 TArray에서 제거
	// 조건: TWeakObjectPtr가 가리키는 객체가 유효하지 않은 경우
	ExplosivesList.RemoveAll([](const TWeakObjectPtr<AExplosive>& Ptr) { return !Ptr.IsValid(); });

	// 폭탄이 3개 모였거나, 이미 3개를 다 던졌던 상태라면 기폭 시도
	if (ExplosivesList.Num() >= MaxBombCount || bIsDetonationReady)
	{
		// 모든 폭탄이 착지했는지 검사
		bool bAllLanded = true;
		for (const TWeakObjectPtr<AExplosive>& WeakExplosive : ExplosivesList)
		{
			if (WeakExplosive.IsValid() && !WeakExplosive->IsAttached())
			{
				bAllLanded = false;
				break;
			}
		}

		if (bAllLanded)
		{
			PerformDetonateAndEnd();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GA_Explosive: Cannot detonate yet. Some bombs are still flying."));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		}

		return;
	}

	// 아직 3개가 안 되었다면 '투척' 모드(조준 프리뷰) 진입

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
		// 폭발물 리스트에 추가
		ExplosivesList.Add(NewExplosive);

		// 폭탄이 3개가 되었다면 '기폭 준비 완료' 상태로 전환
		// 이후 폭탄 하나가 터져서 2개가 되어도 이 플래그는 true이므로 기폭 가능
		if (ExplosivesList.Num() >= 3)
		{
			bIsDetonationReady = true;
		}

		NewExplosive->OnDetonatedDelegate.AddDynamic(this, &UGA_Explosive::OnExplosiveDetonated);

		// GA가 종료된 후에도 액터가 데미지를 줄 수 있도록 SpecHandle을 생성하여 전달
		FGameplayEffectSpecHandle DamageSpecHandle = MakeRuneDamageEffectSpec(CurrentSpecHandle, CurrentActorInfo);

		// 폭발물 초기화
		NewExplosive->Initialize(
			SpawnLoc,
			SavedTargetBlock.Get(),
			1.5f,
			AutoDetonateDelay,
			ExplosionRadius * GetRuneModifiedRange(),
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
	// 폭발물 리스트의 안전한 순회를 위한 복사
	TArray<TWeakObjectPtr<AExplosive>> TempList = ExplosivesList;

	// 원본 리스트는 즉시 비움 (중복 처리 방지 및 깔끔한 상태 유지)
	ExplosivesList.Empty();

	// 리스트에 있는 모든 유효한 폭발물 기폭
	bool bAnyDetonated = false;

	for (TWeakObjectPtr<AExplosive>& Explosive : TempList)
	{
		if (Explosive.IsValid())
		{
			// 폭발 실행 (각 폭발물의 내부 로직 실행)
			Explosive->Detonate();
			bAnyDetonated = true;
		}
	}

	// 폭발물 목록 초기화
	ExplosivesList.Empty();

	if (!bAnyDetonated)
	{
		UE_LOG(LogTemp, Warning, TEXT("GA_Explosive: Detonate requested but no valid bombs found."));
	}

	// 기폭을 수행했으므로 준비 상태 해제
	bIsDetonationReady = false;

	// 기폭 시점에 쿨타임 적용 
	// 3개를 다 던지고 터뜨렸으므로 이제 쿨타임이 돌아야 함
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);

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
	// 폭발물이 터지면 RemoveAll에 의해 삭제됨
	// 예외 상황으로 인해 OnExplosiveDetonated가 호출되지 않은 경우도
	// 다음 OnExplosiveDetonated 호출 시점에 정리됨
	ExplosivesList.RemoveAll([](const TWeakObjectPtr<AExplosive>& Ptr) {
		return !Ptr.IsValid();
		});
	
	// ExplosivesList가 비어있다면 모든 폭발물이 터진 것이므로 쿨타임 시작
	// (기폭 입력을 넣기도 전에 모든 폭발물이 사라진 경우 대비)
	if (ExplosivesList.Num() == 0)
	{
		// 모든 폭탄이 소진되었으므로 준비 상태 해제
		bIsDetonationReady = false;

		// 스킬이 EndAbility로 종료된 상태여도
		// InstancedPerActor 정책이라면 객체는 살아있으므로
		// 쿨타임 Effect를 적용할 수 있음.
		CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);

		UE_LOG(LogTemp, Log, TEXT("GA_Explosive: All bombs detonated/destroyed. Cooldown started."));
	}
}
