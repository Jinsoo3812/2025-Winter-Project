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
#include "SkillManagerComponent.h"

UGA_Destruction::UGA_Destruction() {}

void UGA_Destruction::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

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

	// 룬 범위 배율 가져오기
	float RangeMultiplier = GetRuneModifiedRange() / BaseRange;

	// 룬 배율을 적용한 박스 크기 계산
	FVector AdjustedBoxExtent = BoxExtent * RangeMultiplier;

	// 프리뷰 박스 중심 위치 계산
	// 플레이어 위치에서 마우스 방향으로 (BoxDistance + 박스 X길이) 만큼 떨어진 곳
	FVector BoxCenter = StartLocation + DirectionVector * (BoxDistance + AdjustedBoxExtent.X);

	// 프리뷰 액터 생성 (없을 경우)
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

	// 프리뷰 액터 업데이트 (위치, 회전, 스케일)
	if (RangePreviewActor)
	{
		RangePreviewActor->SetActorLocation(BoxCenter);
		RangePreviewActor->SetActorRotation(LookAtRotation); // 마우스 방향으로 회전

		// 스케일 조정: 기본 큐브 메쉬가 100x100x100이라고 가정
		// BoxExtent는 Half-Size이므로 2배를 곱해 전체 크기를 구하고 100으로 나눔
		FVector NewScale = (AdjustedBoxExtent * 2.0f) / 100.0f;
		RangePreviewActor->SetActorScale3D(NewScale);
	}
}

void UGA_Destruction::OnLeftClickPressed()
{
	// 실제 스킬 시전 시작 알림
	// State.Busy 태그를 부여
	NotifySkillCastStarted();
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
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to commit ability"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

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

	// 룬 범위 배율 가져오기
	float RangeMultiplier = GetRuneModifiedRange() / BaseRange;

	// 룬 배율을 적용한 박스 크기 계산
	FVector AdjustedBoxExtent = BoxExtent * RangeMultiplier;
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
