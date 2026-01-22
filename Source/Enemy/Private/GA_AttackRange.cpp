#include "Enemy/Public/GA_AttackRange.h"
#include "Block/BlockBase.h"      // 블록 클래스 참조
#include "BlockGameplayTags.h"    // 태그 정의 (TAG_Block_Highlight_Target 등)
#include "CollisionChannels.h"

// [필수] 높이 계산 및 디버그 박스 그리기를 위한 헤더
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"

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

	// [안전장치] 공격 중 AI가 회전하거나 이동하지 못하게 고정
	// (위치 계산의 정확도를 높이고 몽타주 끊김 방지)
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
		// 1. Telegraph (예고) 태그 대기 (AnimNotify_SendGameplayEvent에서 보냄)
		WaitTelegraphTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, TelegraphEventTag, nullptr, false, false
		);
		WaitTelegraphTask->EventReceived.AddDynamic(this, &UGA_AttackRange::OnTelegraphEvent);
		WaitTelegraphTask->ReadyForActivation();

		// 2. Hit (타격) 태그 대기
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
	ResetBlockColors(); // 스킬 종료 시 색상 초기화

	MontageTask = nullptr;
	WaitTelegraphTask = nullptr;
	WaitHitTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// =========================================================================
// [1단계: 예고] 위치를 계산하고 저장(Caching)하며, 바닥 색을 바꿈
// =========================================================================
void UGA_AttackRange::OnTelegraphEvent(FGameplayEventData Payload)
{
	ResetBlockColors();

	FVector BoxCenter, BoxExtent;
	// true: 현재 위치 기준으로 계산하고, 그 결과를 'CachedTargetLocation'에 저장해라.
	CalculateAttackBox(BoxCenter, BoxExtent, true);


	// 바닥 블록 감지 (Static, Dynamic 모두 포함)
	TArray<AActor*> OverlappedActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	//ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	//ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Block));

	UKismetSystemLibrary::BoxOverlapActors(
		this, BoxCenter, BoxExtent, ObjectTypes, ABlockBase::StaticClass(),
		{ GetAvatarActorFromActorInfo() }, OverlappedActors
	);


	FGameplayEventData EventData;
	EventData.Instigator = GetAvatarActorFromActorInfo();
	EventData.EventTag = TAG_Block_Highlight_AttackZone;; // "위험" 태그

	for (AActor* Actor : OverlappedActors)
	{
		if (ABlockBase* Block = Cast<ABlockBase>(Actor))
		{
			// [중요] BlockBase에게 이벤트를 보냄 -> BlockConfig 설정을 통해 색상 변경
			Block->HandleGameplayEvent(TAG_Block_Highlight_AttackZone, EventData);
			AffectedBlocks.Add(Block);
		}
	}

	// 몽타주 속도 느리게 (플레이어가 피할 시간 제공)
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Character && Character->GetMesh()->GetAnimInstance() && AttackMontage)
	{
		Character->GetMesh()->GetAnimInstance()->Montage_SetPlayRate(AttackMontage, TelegraphPlayRate);
	}

	// 안전장치 (Hit 이벤트가 안 올 경우를 대비해 일정 시간 후 속도 복구)
	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle_SafetyRestore,
		this,
		&UGA_AttackRange::RestoreMontageSpeed,
		SafetyDuration,
		false
	);
}

// =========================================================================
// [2단계: 타격] 저장된 위치를 불러와서 공격 판정 수행
// =========================================================================
void UGA_AttackRange::OnHitEvent(FGameplayEventData Payload)
{
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_SafetyRestore);
	ExecuteAttack();
}

void UGA_AttackRange::ExecuteAttack()
{
	RestoreMontageSpeed();
	ResetBlockColors(); // 장판 끄기

	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn) return;

	FVector BoxCenter, BoxExtent;
	// false: 아까 저장해둔 위치(CachedTargetLocation)를 그대로 사용해라.
	// (보스가 루트 모션으로 이동했어도 판정 박스는 처음 예고된 자리에 남음)
	CalculateAttackBox(BoxCenter, BoxExtent, false);


	// 플레이어(Pawn) 감지
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
}

// =========================================================================
// [핵심] 위치 계산 + 캐싱 + 발바닥 높이 보정
// =========================================================================
void UGA_AttackRange::CalculateAttackBox(FVector& OutCenter, FVector& OutExtent, bool bIsTelegraph)
{
	// [상황 1: 예고(Telegraph)] 위치를 새로 계산하고 저장(Cache)한다
	if (bIsTelegraph)
	{
		APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
		if (!AvatarPawn) return;

		FVector ForwardDir = AvatarPawn->GetActorForwardVector();
		FVector Origin = AvatarPawn->GetActorLocation(); // 캡슐의 중심 (공중에 떠 있음)

		// [높이 보정] 중심(Origin)에서 캡슐 절반 높이를 빼서 '발바닥(Feet)' 높이를 구함
		float HalfHeight = 88.0f; // 기본값
		if (ACharacter* Character = Cast<ACharacter>(AvatarPawn))
		{
			HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
		float FeetZ = Origin.Z - HalfHeight;

		float CenterDistance = AttackForwardOffset + (AttackRangeForward * 0.5f);

		// 위치 저장! (발바닥 높이 Z를 기억해둠)
		CachedTargetLocation = Origin + (ForwardDir * CenterDistance);
		CachedTargetLocation.Z = FeetZ;

		// 예고 박스: 발바닥 기준 + 50cm 위 (지상 0~100cm 감지)
		OutCenter = CachedTargetLocation;
		OutCenter.Z += 50.0f;

		OutExtent = FVector(AttackRangeForward * 0.5f, AttackWidth * 0.5f, 100.0f);
	}
	// [상황 2: 공격(Attack)] 저장된 위치를 불러온다
	else
	{
		// 아까 저장해둔 위치를 그대로 사용 (루트 모션으로 인한 오차 제거)
		OutCenter = CachedTargetLocation;

		// 공격 박스: 발바닥 기준 + 100cm 위 (지상 0~200cm, 플레이어 피격용)
		OutCenter.Z += 100.0f;

		OutExtent = FVector(AttackRangeForward * 0.5f, AttackWidth * 0.5f, 100.0f);
	}
}

// =========================================================================
// [몽타주 종료] 색상 복구 및 능력 종료
// =========================================================================
void UGA_AttackRange::OnMontageEnded()
{
	ResetBlockColors();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// =========================================================================
// [유틸리티] 몽타주 속도 복구 및 블록 색상 초기화
// =========================================================================
void UGA_AttackRange::RestoreMontageSpeed()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Character && Character->GetMesh()->GetAnimInstance() && AttackMontage)
	{
		Character->GetMesh()->GetAnimInstance()->Montage_SetPlayRate(AttackMontage, 1.0f);
	}
}

// 블록 색상 초기화
void UGA_AttackRange::ResetBlockColors()
{
	FGameplayEventData EventData;
	EventData.Instigator = GetAvatarActorFromActorInfo();
	EventData.EventTag = TAG_Block_Highlight_AttackZone_None; // 복구 태그

	for (TWeakObjectPtr<ABlockBase> BlockPtr : AffectedBlocks)
	{
		if (BlockPtr.IsValid())
		{
			BlockPtr->HandleGameplayEvent(TAG_Block_Highlight_AttackZone_None, EventData);
		}
	}
	AffectedBlocks.Empty();
}