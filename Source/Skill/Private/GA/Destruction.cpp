// Fill out your copyright notice in the Description page of Project Settings.


#include "Destruction.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "BlockSystemInterface.h"
#include "Engine/OverlapResult.h" // FOverlapResult 정의
#include "PreviewTask.h"
#include "CollisionChannels.h"
#include "AbilitySystemBlueprintLibrary.h"

void UDestruction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 1. 프리뷰 모드 진입
	AddGameplayTagToOwner(Tag_State_Preview);
	AddAbilityTag(Tag_Skill_State_Preview);

	// 2. 프리뷰 태스크 시작 (박스 범위 시각화)
	PreviewTask = UPreviewTask::CreatePreviewTask(
		this,
		PreviewRange, // 에디터에서 설정한 Box 범위 전달
		Tag_Highlight_Range,
		Tag_Highlight_Cursor
	);

	if (PreviewTask)
	{
		PreviewTask->ReadyForActivation();
	}

	// 3. 입력 대기 (좌클릭)
	UAbilityTask_WaitGameplayEvent* WaitConfirm = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		Tag_Event_Confirm,
		nullptr,
		false,
		false
	);

	if (WaitConfirm)
	{
		WaitConfirm->EventReceived.AddDynamic(this, &UDestruction::OnConfirmEventReceived);
		WaitConfirm->ReadyForActivation();
	}
}

void UDestruction::OnConfirmEventReceived(FGameplayEventData Payload)
{
	// 1. 필수 요소 확인
	if (!BlockSystem || !DestructionEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Destruction: Missing BlockSystem or GE Class"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	AActor* Avatar = GetActorInfo().AvatarActor.Get();
	UWorld* World = GetWorld();
	if (!Avatar || !World) {
		UE_LOG(LogTemp, Warning, TEXT("Destruction: Invalid Avatar or World"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 2. 타겟 감지
	FCollisionShape CheckShape;
	FVector Center;
	FQuat Rotation;

	// 위치 및 회전 계산
	FVector Origin = Avatar->GetActorLocation();
	FVector ForwardOffset = Avatar->GetActorRotation().RotateVector(PreviewRange.RelativeOffset);
	Center = Origin + ForwardOffset;

	if (PreviewRange.ShapeType == EPreviewShapeType::Box)
	{
		CheckShape = FCollisionShape::MakeBox(PreviewRange.Dimensions);
		Rotation = Avatar->GetActorRotation().Quaternion();
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Destruction: Unsupported Preview Shape Type"));
	}
	// Cylinder 등 추가 가능

	// 물리적 충돌 수행
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Block); // 블록 채널만
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Avatar);

	World->OverlapMultiByObjectType(Overlaps, Center, Rotation, ObjectParams, CheckShape, QueryParams);

	// 3. BlockReference 변환
	TArray<FBlockReference> Targets;
	BlockSystem->GetBlocksFromOverlaps(Overlaps, Targets);

	int32 HitCount = 0;

	// GE Spec 생성 (한 번 만들어서 여러 대상에게 적용)
	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
	ContextHandle.AddSourceObject(Avatar);

	FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(DestructionEffectClass, 1.0f, ContextHandle);

	if (SpecHandle.IsValid())
	{
		for (const FBlockReference& Ref : Targets)
		{
			AActor* TargetActor = Cast<AActor>(Ref.TargetObject.Get());
			if (!TargetActor) continue;

			// 대상의 ASC 가져오기
			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

			if (TargetASC)
			{
				// 파괴 가능 태그(TargetRequiredTag)가 있는지 확인
				if (TargetASC->HasMatchingGameplayTag(TargetRequiredTag))
				{
					// 태그가 있다면 GE 적용 (파괴)
					GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
					HitCount++;
				}
			}
		}
	}

	if (HitCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Destruction: Destroyed %d blocks."), HitCount);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UDestruction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveGameplayTagFromOwner(Tag_State_Preview);
	RemoveAbilityTag(Tag_Skill_State_Preview);

	if (PreviewTask)
	{
		PreviewTask->EndTask();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}