// Fill out your copyright notice in the Description page of Project Settings.

#include "GA/GA_Destruction.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Components/InputComponent.h"

UGA_Destruction::UGA_Destruction() {}

void UGA_Destruction::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo, 
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// CommitAbility: 자원 소모 및 쿨다운 검사
	// 시전 준비 단계에 진입하면 코스트를 지불한 것으로 간주 
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 프리뷰 업데이트 타이머 시작 (매 프레임 갱신)
	if (UWorld* World = GetWorld())
	{
		// 60FPS 간격으로 UpdatePreview 함수 호출
		World->GetTimerManager().SetTimer(TickTimerHandle, this, &UGA_Destruction::UpdatePreview, 0.016f, true);
	}

	// WaitInputPress 어빌리티 태스크 생성
	WaitInputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (WaitInputTask)
	{
		// OnPress 델리게이트에 콜백 함수(스킬 취소) 바인딩
		WaitInputTask->OnPress.AddDynamic(this, &UGA_Destruction::OnCancelPressed);

		// 어빌리티 태스크 활성화
		WaitInputTask->ReadyForActivation();
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_Destruction: Failed to create WaitInputTask"));
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
			PC->InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &UGA_Destruction::OnLeftClickPressed);
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("GA_Destruction: PlayerController or InputComponent is null"));
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_Destruction: OwnerPawn is null"));
	}

	/*
	// CommitAbility: 자원 소모(마나, 스태미너 등) 및 쿨다운을 검사하고, 모두 충족되면 자원을 소모하고 쿨다운을 시작
	// Cost와 Cooldown GE가 설정되어 있다면 이 단계에서 처리됨
	// GA_SkillBase에서 룬이 적용된 쿨타임이 자동으로 적용됨
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// 자원 소모 또는 쿨다운 검사 실패 시 GA 즉시 종료
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 레벨에 존재하는 AvatarActor(플레이어 캐릭터) 가져오기
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Destruction: OwnerActor is null"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 룬이 적용된 범위 값 가져오기
	float ModifiedRange = GetRuneModifiedRange();
	
	// BoxExtent를 범위 배율만큼 조정
	// BaseRange는 BoxExtent의 기준 크기를 의미하므로, 비율을 곱함
	FVector AdjustedBoxExtent = BoxExtent * ModifiedRange;

	// 직육면체 범위의 중심 위치 계산
	// 플레이어의 위치에서 전방 방향으로 (BoxDistance + AdjustedBoxExtent.X) 만큼 이동한 지점이 박스의 중심
	FVector OwnerLocation = AvatarActor->GetActorLocation();
	FVector ForwardVector = AvatarActor->GetActorForwardVector();
	FVector BoxCenter = OwnerLocation + ForwardVector * (BoxDistance + AdjustedBoxExtent.X);

	// 충돌 검사를 위한 파라미터 설정
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(AvatarActor); // 자기 자신은 검사에서 제외

	// 겹친(Overlap) 액터들을 저장할 배열
	TArray<FOverlapResult> OverlapResults;

	// 직육면체 범위 내의 모든 액터를 검사
	// OverlapMultiByChannel: 특정 채널(ECC_Pawn)에서 Box와 겹치는 모든 액터를 찾음
	bool bHit = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		BoxCenter,
		AvatarActor->GetActorQuat(), // 플레이어의 회전값을 사용하여 박스도 같은 방향으로 회전
		ECC_Pawn, // Pawn 채널에 대해 Overlap/Block 한다면 검사
		FCollisionShape::MakeBox(AdjustedBoxExtent), // 룬이 적용된 직육면체 모양 생성
		QueryParams
	);

	// 디버그용 박스 시각화 - 설정된 시간 동안 초록색 박스로 표시
	DrawDebugBox(
		GetWorld(),
		BoxCenter,
		AdjustedBoxExtent,
		AvatarActor->GetActorQuat(),
		FColor::Green,
		false, // 영구적으로 표시하지 않음
		DebugDrawDuration, // 설정된 시간 동안 표시
		0,
		5.0f // 박스 선의 두께
	);

	// 충돌한 액터가 있는 경우 처리
	if (bHit)
	{
		// 겹친 모든 액터에 대해 반복 처리
		for (const FOverlapResult& Overlap : OverlapResults)
		{
			AActor* HitActor = Overlap.GetActor();
			if (!HitActor)
			{
				continue; // 유효하지 않은 액터는 건너뜀
			}

			// 타겟이 AbilitySystemComponent를 가지고 있는지 확인
			// IAbilitySystemInterface를 구현한 액터만 GE를 받을 수 있음
			UAbilitySystemComponent* TargetASC = nullptr;
			if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(HitActor))
			{
				TargetASC = ASI->GetAbilitySystemComponent();
			}

			// 타겟에게 ASC가 있으면 GE 적용
			if (TargetASC)
			{
				// 데미지 Effect 적용 (GA_SkillBase의 DamageEffect, 룬 적용됨)
				FGameplayEffectSpecHandle DamageSpecHandle = MakeRuneDamageEffectSpec(Handle, ActorInfo);
				if (DamageSpecHandle.IsValid())
				{
					GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
						*DamageSpecHandle.Data.Get(),
						TargetASC
					);
				}

				// 파괴 Effect 적용 (GA_Destruction의 DestructionEffect, 블록 파괴용)
				if (DestructionEffect)
				{
					// Gameplay Effect Context 생성 - 이 효과가 어디서 왔는지 등의 정보를 담음
					FGameplayEffectContextHandle DestructionContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
					DestructionContext.AddSourceObject(AvatarActor);

					// Gameplay Effect Spec 생성. Spec은 무거우므로 Handle로 관리
					// Spec: GE는 데이터 에셋, Spec은 그것을 기반한 실제 인스턴스
					// SpecHandle은 무거운 Spec을 래핑하여 관리하도록 지원
					FGameplayEffectSpecHandle DestructionSpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(
						DestructionEffect,
						GetAbilityLevel(), // GA의 현재 레벨을 GE에도 적용
						DestructionContext // GE의 출처 등을 담은 EffectContext
					);

					if (DestructionSpecHandle.IsValid())
					{
						GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
							*DestructionSpecHandle.Data.Get(),
							TargetASC
						);
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("GA_Destruction: Failed to create GameplayEffectSpec for %s"), *HitActor->GetName());
				}
			}
			else
			{
				// ASC가 없는 액터는 GE를 받을 수 없음 (예: 일반 StaticMesh)
				// UE_LOG(LogTemp, Warning, TEXT("GA_Destruction: %s does not have an AbilitySystemComponent"), *HitActor->GetName());
			}
		}
	}
	else
	{
		// 범위 내에 아무 액터도 없었음
		// UE_LOG(LogTemp, Log, TEXT("GA_Destruction: No actors found in destruction range"));
	}

	// GA 종료
	// 4번 인자 bReplicateEndAbility: true - 서버에서 클라이언트로 종료를 복제
	// 5번 인자 bWasCancelled: false - 정상 종료 (취소가 아님)
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	*/
}

void UGA_Destruction::EndAbility(
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
	
	// 타이머 핸들 무효화
	TickTimerHandle.Invalidate();

	// 프리뷰 액터 제거
	if (RangePreviewActor)
	{
		RangePreviewActor->Destroy();
		RangePreviewActor = nullptr;
	}

	// 태스크 정리
	if (WaitInputTask)
	{
		WaitInputTask->EndTask();
		WaitInputTask = nullptr;
	}

	// 입력 바인딩 해제
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			// 현재 객체에 바인딩된 델리게이트 제거
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

void UGA_Destruction::UpdatePreview()
{
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn) {
		UE_LOG(LogTemp, Error, TEXT("GA_Destruction: OwnerPawn is null in UpdatePreview"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) {
		UE_LOG(LogTemp, Error, TEXT("GA_Destruction: PlayerController is null in UpdatePreview"));
		return;
	}

	// 마우스 커서 위치 가져오기
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Visibility, true, HitResult);

	// 마우스가 유효한 위치를 가리키고 있어야 함
	if (!HitResult.bBlockingHit) return;

	// 방향 벡터 및 회전 계산
	FVector StartLocation = OwnerPawn->GetActorLocation();
	FVector TargetLocation = HitResult.Location;

	// 높이(Z) 차이는 무시하고 수평 방향만 고려 (탑다운 뷰이므로)
	TargetLocation.Z = StartLocation.Z;

	// 플레이어 위치에서 마우스 위치로 향하는 단위 벡터
	FVector DirectionVector = (TargetLocation - StartLocation).GetSafeNormal();

	// 해당 방향을 바라보는 회전값 생성 (FRotationMatrix::MakeFromX 활용)
	FRotator LookAtRotation = FRotationMatrix::MakeFromX(DirectionVector).Rotator();

	// 룬이 적용된 범위 및 크기 계산
	float ModifiedRange = GetRuneModifiedRange();

	// BaseRange가 0이면 나눗셈 오류가 발생하므로 안전장치
	float RangeRatio = (BaseRange > 0.0f) ? (ModifiedRange / BaseRange) : 1.0f;
	FVector AdjustedBoxExtent = BoxExtent * RangeRatio;

	// 프리뷰 박스 중심 위치 계산
	// 플레이어 위치에서 마우스 방향으로 (BoxDistance + 박스 X길이) 만큼 떨어진 곳
	FVector BoxCenter = StartLocation + DirectionVector * (BoxDistance + AdjustedBoxExtent.X);

	// 5. 프리뷰 액터 생성 (없을 경우)
	if (!RangePreviewActor && RangePreviewActorClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		RangePreviewActor = GetWorld()->SpawnActor<AActor>(
			RangePreviewActorClass,
			BoxCenter,
			LookAtRotation,
			SpawnParams
		);

		if (RangePreviewActor)
		{
			RangePreviewActor->SetActorEnableCollision(false); // 충돌 비활성화
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("GA_Destruction: Failed to spawn RangePreviewActor"));
		}
	}

	// 6. 프리뷰 액터 업데이트 (위치, 회전, 스케일)
	if (RangePreviewActor)
	{
		RangePreviewActor->SetActorLocation(BoxCenter);
		RangePreviewActor->SetActorRotation(LookAtRotation); // 마우스 방향으로 회전

		// 스케일 조정: 기본 큐브 메쉬가 100x100x100이라고 가정
		// BoxExtent는 Half-Size이므로 2배를 곱해 전체 크기를 구하고 100으로 나눔
		// 뭔소린지 모르겠음;
		FVector NewScale = (AdjustedBoxExtent * 2.0f) / 100.0f;
		RangePreviewActor->SetActorScale3D(NewScale);
	}
}

void UGA_Destruction::OnLeftClickPressed()
{
	// 좌클릭 시 파괴 로직 수행
	PerformDestruction();
}

void UGA_Destruction::OnCancelPressed(float TimeWaited)
{
	// 스킬 키 재입력 시 취소
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UGA_Destruction::PerformDestruction()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 마우스 방향 및 회전 계산
	FVector DirectionVector = AvatarActor->GetActorForwardVector(); // 기본값 (실패 시)
	FQuat BoxRotation = AvatarActor->GetActorQuat(); // 기본값 (실패 시)

	if (APawn* OwnerPawn = Cast<APawn>(AvatarActor))
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			FHitResult HitResult;
			PC->GetHitResultUnderCursor(ECC_Visibility, true, HitResult);

			if (HitResult.bBlockingHit)
			{
				FVector StartLocation = OwnerPawn->GetActorLocation();
				FVector TargetLocation = HitResult.Location;
				TargetLocation.Z = StartLocation.Z; // 높이는 무시

				DirectionVector = (TargetLocation - StartLocation).GetSafeNormal();
				BoxRotation = FRotationMatrix::MakeFromX(DirectionVector).ToQuat();
			}
			
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("GA_Destruction: PlayerController is null in PerformDestruction"));
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_Destruction: OwnerPawn is null in PerformDestruction"));
	}

	// 범위 재계산
	float ModifiedRange = GetRuneModifiedRange();
	FVector AdjustedBoxExtent = BoxExtent * (ModifiedRange / BaseRange);
	FVector OwnerLocation = AvatarActor->GetActorLocation();

	// 마우스 방향(DirectionVector)을 사용하여 중심점 계산
	FVector BoxCenter = OwnerLocation + DirectionVector * (BoxDistance + AdjustedBoxExtent.X);

	// 충돌 검사 파라미터
	FCollisionQueryParams QueryParams;
	// 본인과의 충돌 제외
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FOverlapResult> OverlapResults;

	// 충돌 검사 수행
	bool bHit = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		BoxCenter,
		BoxRotation, // 마우스 방향에 맞춘 회전값 적용
		ECC_Pawn,
		FCollisionShape::MakeBox(AdjustedBoxExtent),
		QueryParams
	);

	if (bHit)
	{
		for (const FOverlapResult& Overlap : OverlapResults)
		{
			AActor* HitActor = Overlap.GetActor();
			if (!HitActor) continue;

			// ASC 확인
			// ASC를 가진 액터만 GE 적용 가능
			UAbilitySystemComponent* TargetASC = nullptr;
			if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(HitActor))
			{
				TargetASC = ASI->GetAbilitySystemComponent();
			}

			if (TargetASC)
			{
				// 데미지 Effect 적용
				FGameplayEffectSpecHandle DamageSpecHandle = MakeRuneDamageEffectSpec(CurrentSpecHandle, CurrentActorInfo);
				if (DamageSpecHandle.IsValid())
				{
					GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
						*DamageSpecHandle.Data.Get(),
						TargetASC
					);
				}
				else {
					UE_LOG(LogTemp, Error, TEXT("GA_Destruction: Failed to create GameplayEffectSpec for %s"), *HitActor->GetName());
				}

				// 파괴 Effect 적용
				if (DestructionEffect)
				{
					FGameplayEffectContextHandle DestructionContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
					DestructionContext.AddSourceObject(AvatarActor);

					FGameplayEffectSpecHandle DestructionSpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(
						DestructionEffect,
						GetAbilityLevel(),
						DestructionContext
					);

					if (DestructionSpecHandle.IsValid())
					{
						GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
							*DestructionSpecHandle.Data.Get(),
							TargetASC
						);
					}
					else {
						UE_LOG(LogTemp, Error, TEXT("GA_Destruction: Failed to create GameplayEffectSpec for %s"), *HitActor->GetName());
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("GA_Destruction: DestructionEffect is null"));
				}
			}
		}
	}

	// 로직 수행 완료 후 정상 종료 (bWasCancelled = false)
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
