#include "GA_AttackRange.h" // 내 헤더 (Public 폴더 경로 포함)
#include "Block/BlockBase.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameFramework/Character.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

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

	// 1. 몽타주 재생
	if (AttackMontage)
	{
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, AttackMontage, 1.0f, NAME_None, false
		);

		// [오류 수정 1] 인자가 없는 함수(OnMontageFinished)에 연결
		Task->OnCompleted.AddDynamic(this, &UGA_AttackRange::OnMontageFinished);
		Task->OnInterrupted.AddDynamic(this, &UGA_AttackRange::OnMontageFinished);
		Task->OnBlendOut.AddDynamic(this, &UGA_AttackRange::OnMontageFinished);

		Task->ReadyForActivation();
	}

	// 2. 공격 범위 계산
	FVector ForwardDir = AvatarPawn->GetActorForwardVector();
	FVector Origin = AvatarPawn->GetActorLocation();

	FVector BoxCenter = Origin + (ForwardDir * (AttackRangeForward * 0.5f));
	BoxCenter.Z -= 50.0f;

	FVector BoxExtent = FVector(AttackRangeForward * 0.5f, AttackWidth * 0.5f, 50.0f);

	TArray<AActor*> OverlappedActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

	UKismetSystemLibrary::BoxOverlapActors(
		this, BoxCenter, BoxExtent, ObjectTypes, ABlockBase::StaticClass(),
		{ AvatarPawn }, OverlappedActors
	);

	AffectedBlocks.Empty();

	// 3. 색상 변경
	for (AActor* Actor : OverlappedActors)
	{
		if (ABlockBase* Block = Cast<ABlockBase>(Actor))
		{
			Block->SetHighlightState(EBlockHighlightState::Danger);
			AffectedBlocks.Add(Block);
		}
	}

	// 4. 대기
	UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, TelegraphDuration);
	if (DelayTask)
	{
		DelayTask->OnFinish.AddDynamic(this, &UGA_AttackRange::ExecuteAttack);
		DelayTask->ReadyForActivation();
	}
}

void UGA_AttackRange::ExecuteAttack()
{
	ResetBlockColors();

	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn) return;

	// 공격 범위 재계산 (플레이어 타격용 높이 조절)
	FVector ForwardDir = AvatarPawn->GetActorForwardVector();
	FVector Origin = AvatarPawn->GetActorLocation();

	FVector BoxCenter = Origin + (ForwardDir * (AttackRangeForward * 0.5f));
	BoxCenter.Z += 50.0f; // 바닥 위

	FVector BoxExtent = FVector(AttackRangeForward * 0.5f, AttackWidth * 0.5f, 100.0f);

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
						// TargetASC에게 "나 자신에게 이 스펙을 적용하라"고 명령
						TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
					}
				}
			}
		}
	}

	//몽타주 없으면 종료
	if (!AttackMontage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_AttackRange::OnMontageFinished()
{
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