#include "Enemy/Public/GA_AttackRange.h"
#include "Block/BlockBase.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameFramework/Character.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h" // 이동 제어용

UGA_AttackRange::UGA_AttackRange()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_AttackRange::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// [디버그] 실행 확인용 로그
	// UE_LOG(LogTemp, Warning, TEXT("[GAS] Ability ACTIVATED: %s"), *GetName());

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APawn* AvatarPawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
	if (!AvatarPawn)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// [안전 장치] 스킬 시전 중 이동 방지 (미끄러짐 및 캔슬 방지)
	if (AController* Controller = AvatarPawn->GetController())
	{
		Controller->StopMovement();
	}

	// =================================================================
	// [분기 A] 몽타주 기반 공격 (메인 로직)
	// =================================================================
	if (AttackMontage)
	{
		// 1. 몽타주 재생 태스크 생성 (멤버 변수에 할당!)
		// [중요] 앞에 'UAbilityTask_PlayMontageAndWait* ' 타입을 쓰지 않습니다.
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, AttackMontage, TelegraphPlayRate, NAME_None, false
		);

		// 몽타주 종료/취소 시 처리 연결
		MontageTask->OnCompleted.AddDynamic(this, &UGA_AttackRange::OnMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_AttackRange::OnMontageFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &UGA_AttackRange::OnMontageFinished);
		MontageTask->ReadyForActivation();

		// 2. Hit 이벤트 대기 태스크 (멤버 변수에 할당!)
		WaitHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, HitEventTag, nullptr, false, false
		);
		WaitHitTask->EventReceived.AddDynamic(this, &UGA_AttackRange::OnHitEventReceived);
		WaitHitTask->ReadyForActivation();

		// 3. Telegraph 이벤트 대기 태스크 (멤버 변수에 할당!)
		WaitTelegraphTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, TelegraphEventTag, nullptr, false, false
		);
		WaitTelegraphTask->EventReceived.AddDynamic(this, &UGA_AttackRange::EnableTelegraph);
		WaitTelegraphTask->ReadyForActivation();

		// 4. [즉시 실행] 첫 타격 예고 장판 켜기
		// 노티파이(Notify)를 기다리지 않고 강제로 실행하여 "가끔 안 켜지는" 버그 방지
		FGameplayEventData DummyData;
		EnableTelegraph(DummyData);

		// 속도 복구 타이머 시작
		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle_SpeedUp,
			this,
			&UGA_AttackRange::RestoreMontageSpeed,
			TelegraphDuration,
			false
		);
	}
	// =================================================================
	// [분기 B] 몽타주 없는 경우 (타이머 기반)
	// =================================================================
	else
	{
		FGameplayEventData DummyData;
		EnableTelegraph(DummyData);

		UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, TelegraphDuration);
		DelayTask->OnFinish.AddDynamic(this, &UGA_AttackRange::ExecuteAttack);
		DelayTask->ReadyForActivation();
	}
}

void UGA_AttackRange::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// [중요] 어빌리티 종료 시 정리 작업

	// 1. 타이머 제거
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_SpeedUp);

	// 2. 장판 색상 초기화
	ResetBlockColors();

	// 3. 태스크 포인터 초기화 (GC가 수거해갈 수 있도록 놓아줌)
	MontageTask = nullptr;
	WaitHitTask = nullptr;
	WaitTelegraphTask = nullptr;

	// 4. 부모 함수 호출 (필수)
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_AttackRange::EnableTelegraph(FGameplayEventData Payload)
{
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn) return;

	ResetBlockColors();

	// 1. 위치 계산 (캐싱)
	FVector ForwardDir = AvatarPawn->GetActorForwardVector();
	FVector Origin = AvatarPawn->GetActorLocation();
	float HalfLength = AttackRangeForward * 0.5f;

	CachedTargetLocation = Origin + (ForwardDir * (AttackForwardOffset + HalfLength));

	// 2. 판정 박스 설정 (바닥 감지 강화)
	FVector BoxCenter = CachedTargetLocation;
	BoxCenter.Z -= 100.0f; // 바닥 아래로 깊숙이
	FVector BoxExtent = FVector(HalfLength, AttackWidth * 0.5f, 150.0f); // 두껍게

	// 3. 오버랩 검사
	TArray<AActor*> OverlappedActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

	UKismetSystemLibrary::BoxOverlapActors(
		this, BoxCenter, BoxExtent, ObjectTypes, ABlockBase::StaticClass(),
		{ AvatarPawn }, OverlappedActors
	);

	for (AActor* Actor : OverlappedActors)
	{
		if (ABlockBase* Block = Cast<ABlockBase>(Actor))
		{
			Block->SetHighlightState(EBlockHighlightState::Danger);
			AffectedBlocks.Add(Block);
		}
	}

	// 4. [중요] 몽타주 속도 조절 (조건 없이 강제 실행)
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Character && Character->GetMesh()->GetAnimInstance() && AttackMontage)
	{
		UAnimInstance* AnimInst = Character->GetMesh()->GetAnimInstance();

		// IsPlaying 체크 없이 강제로 설정하여 타이밍 이슈 방지
		AnimInst->Montage_SetPlayRate(AttackMontage, TelegraphPlayRate);

		// 타이머 재설정 (N연타 고려)
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_SpeedUp);
		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle_SpeedUp,
			this,
			&UGA_AttackRange::RestoreMontageSpeed,
			TelegraphDuration,
			false
		);
	}
}

void UGA_AttackRange::OnHitEventReceived(FGameplayEventData Payload)
{
	ExecuteAttack();
	// EndAbility 호출 안 함 (콤보 대기)
}

void UGA_AttackRange::ExecuteAttack()
{
	ResetBlockColors();

	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn) return;

	// 판정 박스 설정 (공격용: 위로 올림)
	FVector BoxCenter = CachedTargetLocation;
	BoxCenter.Z += 50.0f;
	FVector BoxExtent = FVector(AttackRangeForward * 0.5f, AttackWidth * 0.5f, 100.0f);

	// [디버그] 초록 박스 표시
	/*
	DrawDebugBox(
		GetWorld(), BoxCenter, BoxExtent, FColor::Green, false, 1.0f
	);
	*/

	TArray<AActor*> OverlappedPawns;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	UKismetSystemLibrary::BoxOverlapActors(
		this, BoxCenter, BoxExtent, ObjectTypes, APawn::StaticClass(),
		{ AvatarPawn }, OverlappedPawns
	);

	// 데미지 적용
	if (DamageEffectClass)
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass);
		if (SpecHandle.IsValid())
		{
			for (AActor* TargetActor : OverlappedPawns)
			{
				if (TargetActor && TargetActor != AvatarPawn)
				{
					UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
					if (TargetASC)
					{
						TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
					}
				}
			}
		}
	}

	// 몽타주 없으면 바로 종료
	if (!AttackMontage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_AttackRange::OnMontageFinished()
{
	ResetBlockColors();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_AttackRange::ResetBlockColors()
{
	for (TWeakObjectPtr<ABlockBase> BlockPtr : AffectedBlocks)
	{
		if (BlockPtr.IsValid())
		{
			BlockPtr->SetHighlightState(EBlockHighlightState::None);
		}
	}
	AffectedBlocks.Empty();
}

void UGA_AttackRange::RestoreMontageSpeed()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Character && Character->GetMesh()->GetAnimInstance() && AttackMontage)
	{
		Character->GetMesh()->GetAnimInstance()->Montage_SetPlayRate(AttackMontage, 1.0f);
	}
}