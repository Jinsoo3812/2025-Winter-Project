#include "Enemy/Public/GA_AttackRange.h"
#include "Block/BlockBase.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameFramework/Character.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"

UGA_AttackRange::UGA_AttackRange()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UGA_AttackRange::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APawn* AvatarPawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
	if (!AvatarPawn)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// AI 시선 고정 (이건 여전히 유용합니다. 공격 중 딴청 피우기 방지)
	if (AController* Controller = AvatarPawn->GetController())
	{
		Controller->StopMovement();
		if (AAIController* AIC = Cast<AAIController>(Controller))
		{
			AIC->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}

	if (AttackMontage)
	{
		// 1. 리스너 먼저 등록 (안정성)
		WaitTelegraphTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, TelegraphEventTag, nullptr, false, false);
		WaitTelegraphTask->EventReceived.AddDynamic(this, &UGA_AttackRange::OnTelegraphEvent);
		WaitTelegraphTask->ReadyForActivation();

		WaitHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag, nullptr, false, false);
		WaitHitTask->EventReceived.AddDynamic(this, &UGA_AttackRange::OnHitEvent);
		WaitHitTask->ReadyForActivation();

		// 2. 몽타주 재생
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, AttackMontage, 1.0f, NAME_None, false
		);
		MontageTask->OnCompleted.AddDynamic(this, &UGA_AttackRange::OnMontageEnded);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_AttackRange::OnMontageEnded);
		MontageTask->OnBlendOut.AddDynamic(this, &UGA_AttackRange::OnMontageEnded);
		MontageTask->ReadyForActivation();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGA_AttackRange::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_SafetyRestore);
	ResetBlockColors();

	MontageTask = nullptr;
	WaitTelegraphTask = nullptr;
	WaitHitTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_AttackRange::OnTelegraphEvent(FGameplayEventData Payload)
{
	ResetBlockColors();

	// 1. 박스 계산 (Standard)
	FVector BoxCenter, BoxExtent;
	CalculateAttackBox(BoxCenter, BoxExtent, true);

	TArray<AActor*> OverlappedActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

	UKismetSystemLibrary::BoxOverlapActors(
		this, BoxCenter, BoxExtent, ObjectTypes, ABlockBase::StaticClass(),
		{ GetAvatarActorFromActorInfo() }, OverlappedActors
	);

	for (AActor* Actor : OverlappedActors)
	{
		if (ABlockBase* Block = Cast<ABlockBase>(Actor))
		{
			Block->SetHighlightState(EBlockHighlightState::Danger);
			AffectedBlocks.Add(Block);
		}
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Character && Character->GetMesh()->GetAnimInstance() && AttackMontage)
	{
		Character->GetMesh()->GetAnimInstance()->Montage_SetPlayRate(AttackMontage, TelegraphPlayRate);
	}

	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle_SafetyRestore, this, &UGA_AttackRange::RestoreMontageSpeed, SafetyDuration, false
	);
}

void UGA_AttackRange::OnHitEvent(FGameplayEventData Payload)
{
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_SafetyRestore);
	ExecuteAttack();
}

void UGA_AttackRange::ExecuteAttack()
{
	RestoreMontageSpeed();
	ResetBlockColors();

	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn) return;

	FVector BoxCenter, BoxExtent;
	CalculateAttackBox(BoxCenter, BoxExtent, false);

	TArray<AActor*> OverlappedPawns;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	UKismetSystemLibrary::BoxOverlapActors(
		this, BoxCenter, BoxExtent, ObjectTypes, APawn::StaticClass(),
		{ AvatarPawn }, OverlappedPawns
	);

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
}

// 공격범위	박스 계산 함수 (Standard)
void UGA_AttackRange::CalculateAttackBox(FVector& OutCenter, FVector& OutExtent, bool bIsTelegraph)
{
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn) return;

	FVector ForwardDir = AvatarPawn->GetActorForwardVector();
	FVector Origin = AvatarPawn->GetActorLocation();

	// [Standard Math]
	// "보스 중심에서 Offset만큼 떨어진 곳부터, Range만큼 뻗어나가는 박스"

	// 박스의 중심점 거리 = 오프셋 + (사거리 / 2)
	float CenterDistance = AttackForwardOffset + (AttackRangeForward * 0.5f);

	CachedTargetLocation = Origin + (ForwardDir * CenterDistance);
	OutCenter = CachedTargetLocation;

	if (bIsTelegraph)
	{
		// 바닥 감지용: 박스를 살짝 아래로 (-100)
		OutCenter.Z -= 100.0f;
		OutExtent = FVector(AttackRangeForward * 0.5f, AttackWidth * 0.5f, 150.0f);
	}
	else
	{
		// 공격 판정용: 박스를 살짝 위로 (+50)
		OutCenter.Z += 50.0f;
		OutExtent = FVector(AttackRangeForward * 0.5f, AttackWidth * 0.5f, 100.0f);
	}
}

void UGA_AttackRange::OnMontageEnded()
{
	ResetBlockColors();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_AttackRange::RestoreMontageSpeed()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Character && Character->GetMesh()->GetAnimInstance() && AttackMontage)
	{
		Character->GetMesh()->GetAnimInstance()->Montage_SetPlayRate(AttackMontage, 1.0f);
	}
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