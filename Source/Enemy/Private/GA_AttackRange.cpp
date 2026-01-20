#include "Enemy/Public/GA_AttackRange.h"
#include "Block/BlockBase.h" 
#include "BlockGameplayTags.h" 
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

	// AI 시선 고정
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
		// 1. Telegraph 대기
		WaitTelegraphTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, TelegraphEventTag, nullptr, false, false
		);
		WaitTelegraphTask->EventReceived.AddDynamic(this, &UGA_AttackRange::OnTelegraphEvent);
		WaitTelegraphTask->ReadyForActivation();

		// 2. Hit 대기
		WaitHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, HitEventTag, nullptr, false, false
		);
		WaitHitTask->EventReceived.AddDynamic(this, &UGA_AttackRange::OnHitEvent);
		WaitHitTask->ReadyForActivation();

		// 3. 몽타주 재생
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

// [이벤트 수신] 예고 신호
void UGA_AttackRange::OnTelegraphEvent(FGameplayEventData Payload)
{
	ResetBlockColors();

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
		// 1. 블록 캐스팅
		if (ABlockBase* Block = Cast<ABlockBase>(Actor))
		{
			// 2. 인터페이스를 통해 이벤트 전달
			// IGameplayEventInterface는 ABlockBase가 상속받았으므로 바로 호출 가능
			// Target 태그가 "빨간색/위험"을 의미한다고 가정합니다.

			// 페이로드 준비 (누가 보냈는지 등)
			FGameplayEventData EventData;
			EventData.Instigator = GetAvatarActorFromActorInfo();
			EventData.EventTag = TAG_Block_Highlight_Target;

			Block->HandleGameplayEvent(TAG_Block_Highlight_Target, EventData);

			AffectedBlocks.Add(Block);
		}
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Character && Character->GetMesh()->GetAnimInstance() && AttackMontage)
	{
		Character->GetMesh()->GetAnimInstance()->Montage_SetPlayRate(AttackMontage, TelegraphPlayRate);
	}

	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle_SafetyRestore,
		this,
		&UGA_AttackRange::RestoreMontageSpeed,
		SafetyDuration,
		false
	);
}

// [이벤트 수신] 타격 신호
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

void UGA_AttackRange::CalculateAttackBox(FVector& OutCenter, FVector& OutExtent, bool bIsTelegraph)
{
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn) return;

	FVector ForwardDir = AvatarPawn->GetActorForwardVector();
	FVector Origin = AvatarPawn->GetActorLocation();

	float CenterDistance = AttackForwardOffset + (AttackRangeForward * 0.5f);

	CachedTargetLocation = Origin + (ForwardDir * CenterDistance);
	OutCenter = CachedTargetLocation;

	if (bIsTelegraph)
	{
		OutCenter.Z -= 50.0f;
		OutExtent = FVector(AttackRangeForward * 0.5f, AttackWidth * 0.5f, 100.0f);
	}
	else
	{
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

// [색상 초기화] 태그 방식 적용
void UGA_AttackRange::ResetBlockColors()
{
	FGameplayEventData EventData;
	EventData.Instigator = GetAvatarActorFromActorInfo();
	// None 태그 = 원래대로 복구
	EventData.EventTag = TAG_Block_Highlight_None;

	for (TWeakObjectPtr<ABlockBase> BlockPtr : AffectedBlocks)
	{
		if (BlockPtr.IsValid())
		{
			// HandleGameplayEvent 호출 (색상 끄기)
			BlockPtr->HandleGameplayEvent(TAG_Block_Highlight_None, EventData);
		}
	}
	AffectedBlocks.Empty();
}