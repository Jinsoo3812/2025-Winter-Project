#include "Enemy/Public/GA_AttackRange.h" // 내 헤더 파일
#include "Block/BlockBase.h"             // 바닥 블록 클래스 (World 모듈)
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameFramework/Character.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"

// Block 색상 변경을 위해 필요한 헤더들 추가했습니다.
#include "BlockGameplayTags.h"
#include "GameplayEventInterface.h"

UGA_AttackRange::UGA_AttackRange()
{
	// 어빌리티 인스턴싱: 액터마다 상태를 별도로 관리 (변수 저장 필수)
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 서버 실행 정책: 데미지 판정은 서버 권한 필요
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

	// =================================================================
	// [분기 A] 몽타주가 있는 경우 (N연타 콤보 로직)
	// =================================================================
	if (AttackMontage)
	{

		// 1. 몽타주 재생 시작
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, AttackMontage, TelegraphPlayRate, NAME_None, false
		);

		// 몽타주가 끝나거나 중단되면 스킬을 완전히 종료(EndAbility)
		MontageTask->OnCompleted.AddDynamic(this, &UGA_AttackRange::OnMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_AttackRange::OnMontageFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &UGA_AttackRange::OnMontageFinished);
		MontageTask->ReadyForActivation();

		// 2. Hit이벤트 대기 (무한 반복)
		// OnlyTriggerOnce = false로 설정하여 2타, 3타 신호도 계속 받음
		UAbilityTask_WaitGameplayEvent* WaitHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			HitEventTag, // 예: Event.Montage.Hit
			nullptr,
			false, // OnlyTriggerOnce = false (중요!)
			false
		);
		WaitHitTask->EventReceived.AddDynamic(this, &UGA_AttackRange::OnHitEventReceived);
		WaitHitTask->ReadyForActivation();

		// 3. Telegraph 이벤트 대기 (무한 반복)
		// 2타, 3타 공격 준비 신호를 받음
		UAbilityTask_WaitGameplayEvent* WaitTelegraphTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			TelegraphEventTag, // 예: Event.Montage.Telegraph
			nullptr,
			false, // OnlyTriggerOnce = false (중요!)
			false
		);
		WaitTelegraphTask->EventReceived.AddDynamic(this, &UGA_AttackRange::EnableTelegraph);
		WaitTelegraphTask->ReadyForActivation();

		// 4. [옵션] 첫타 장판 즉시 켜기
		// 몽타주 0.0초에 노티파이가 없을 수도 있으니, 안전하게 1타 장판은 바로 켭니다.
		FGameplayEventData DummyData;
		EnableTelegraph(DummyData);

		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle_SpeedUp,
			this,
			&UGA_AttackRange::RestoreMontageSpeed,
			TelegraphDuration, // n초 뒤 실행
			false
		);
	}
	// =================================================================
	// [분기 B] 몽타주가 없는 경우 (단순 타이머 로직 - 구버전 호환)
	// =================================================================
	else
	{
		FGameplayEventData DummyData;
		EnableTelegraph(DummyData); // 장판 켜기

		// 시간 대기 후 공격 실행
		UAbilityTask_WaitDelay* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, TelegraphDuration);
		DelayTask->OnFinish.AddDynamic(this, &UGA_AttackRange::ExecuteAttack); // 여기서는 ExecuteAttack이 종료까지 담당해야 함 (주의)
		DelayTask->ReadyForActivation();
	}
}

// [함수 1] 바닥 빨간 장판 켜기 (2타, 3타 때 재활용됨)
void UGA_AttackRange::EnableTelegraph(FGameplayEventData Payload)
{
	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn) return;

	// 혹시 켜져있는 색상이 있다면 초기화 (깜빡임 방지)
	ResetBlockColors();

	// 1. 현재 위치 기준으로 범위 박스 계산
	// (Root Motion으로 보스가 이동했을 수 있으므로 매번 새로 계산합니다)
	FVector ForwardDir = AvatarPawn->GetActorForwardVector();
	FVector Origin = AvatarPawn->GetActorLocation();

	float HalfLength = AttackRangeForward * 0.5f;

	CachedTargetLocation = Origin + (ForwardDir * (AttackForwardOffset + HalfLength));
	FVector BoxCenter = CachedTargetLocation;

	// 바닥 감지용 높이 보정 (-50)
	BoxCenter.Z -= 50.0f;

	FVector BoxExtent = FVector(HalfLength, AttackWidth * 0.5f, 50.0f);

	// 2. 오버랩 검사 (WorldStatic = 블록)
	TArray<AActor*> OverlappedActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

	UKismetSystemLibrary::BoxOverlapActors(
		this, BoxCenter, BoxExtent, ObjectTypes, ABlockBase::StaticClass(),
		{ AvatarPawn }, OverlappedActors
	);

	// 3. 색상 변경 및 저장
	for (AActor* Actor : OverlappedActors)
	{
		// 인터페이스 구현 여부 검사
		IGameplayEventInterface* InterfaceObj = Cast<IGameplayEventInterface>(Actor);
		if (InterfaceObj)
		{
			// 이벤트 전송
			InterfaceObj->HandleGameplayEvent(TAG_Block_Highlight_Target, Payload);
			AffectedBlocks.Add(Cast<ABlockBase>(Actor));
		}

		/*
		* 기존 코드
		* 이제 BlockBase가 IGameplayEventInterface를 구현하므로 Enemy 모듈도 World 모듈에 대한 의존을 끊으셔도 됩니다. (현재는 Block에 대한 의존성을 유지하고 있습니다. 나중에 시간 나면 제거하세요)
		* 현재 Core 모듈의 BlockGameplayTag.h에 TAG_Block_Highlight_Danger 라는 태그가 선언되어 있지 않습니다.
		* 이를 직접 정의하시고(BlockGameplayTags.h 참고), BlockGameplayTags.cpp에서 해당 태그를 정의해주셔야 합니다.
		* 또한 에디터의 Block 폴더의 Block Config 라는 Data Asset이 있습니다. 그곳에서 Block.Highlight.Danger 태그에 대한 CPD 값도 설정해주셔야 합니다.
		* 마지막으로 Material 에서도 해당 CPD에 맞는 색상 처리가 되어 있어야 합니다.(아마 제 리팩토링이 들어가면서 Dange에 대한 CPD 처리가 지워졌을겁니다.)
		*/
		/*
		if (ABlockBase* Block = Cast<ABlockBase>(Actor))
		{
			Block->SetHighlightState(EBlockHighlightState::Danger);
			AffectedBlocks.Add(Block);
		}
		*/
	}

	// 몽타주 속도를 다시 늦춤 (관성 느낌)
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Character && Character->GetMesh()->GetAnimInstance())
	{
		UAnimInstance* AnimInst = Character->GetMesh()->GetAnimInstance();
		if (AnimInst->Montage_IsPlaying(AttackMontage))
		{
			AnimInst->Montage_SetPlayRate(AttackMontage, TelegraphPlayRate);

			// 다시 n초 뒤에 빨라지도록 타이머 재설정
			GetWorld()->GetTimerManager().SetTimer(
				TimerHandle_SpeedUp,
				this,
				&UGA_AttackRange::RestoreMontageSpeed,
				TelegraphDuration,
				false
			);
		}
	}
}

// [함수 2] 몽타주에서 'Hit' 신호가 오면 호출
void UGA_AttackRange::OnHitEventReceived(FGameplayEventData Payload)
{
	// 실제 공격 수행 (데미지 + 장판 끄기)
	ExecuteAttack();

	// [중요] 여기서 EndAbility를 호출하지 않습니다!
	// 몽타주가 남았다면 다음 공격(2타)을 위해 기다려야 하기 때문입니다.
}

// [함수 3] 실제 데미지 적용 및 장판 끄기
void UGA_AttackRange::ExecuteAttack()
{
	// 1. 바닥 장판 끄기
	ResetBlockColors();

	APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!AvatarPawn) return;

	FVector BoxCenter = CachedTargetLocation;
	
	// 플레이어 몸통 높이 보정 (+50)
	BoxCenter.Z += 50.0f;

	// 높이(Z)를 넉넉하게(100) 주어 점프한 플레이어도 맞게 설정
	float HalfLength = AttackRangeForward * 0.5f;
	FVector BoxExtent = FVector(HalfLength, AttackWidth * 0.5f, 100.0f);

	// ▼▼▼ [디버그 추가] 공격 판정 박스를 화면에 2초간 그립니다 ▼▼▼
	DrawDebugBox(
		GetWorld(),
		BoxCenter,
		BoxExtent,
		FColor::Green, // 타격 박스는 초록색
		false,
		2.0f
	);
	// ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲


	TArray<AActor*> OverlappedPawns;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	// 3. 적(Pawn) 감지
	UKismetSystemLibrary::BoxOverlapActors(
		this, BoxCenter, BoxExtent, ObjectTypes, APawn::StaticClass(),
		{ AvatarPawn }, OverlappedPawns
	);


	// 4. 데미지(GE) 적용
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

	// (몽타주가 없는 예외적인 경우에만 여기서 종료)
	if (!AttackMontage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

// [함수 4] 몽타주가 완전히 끝나면 호출 (스킬 종료)
void UGA_AttackRange::OnMontageFinished()
{
	// 장판이 켜진 채로 끝났을 수도 있으니 정리
	ResetBlockColors();

	// 진짜 종료
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// [헬퍼] 블록 색상 초기화
void UGA_AttackRange::ResetBlockColors()
{
	FGameplayEventData Payload;
	Payload.EventTag = TAG_Block_Highlight_None;
	Payload.Instigator = GetAvatarActorFromActorInfo();
	Payload.EventMagnitude = 0.0f; // 안 씀

	for (TWeakObjectPtr<ABlockBase> BlockPtr : AffectedBlocks)
	{
		// 인터페이스 구현 여부 검사
		IGameplayEventInterface* InterfaceObj = Cast<IGameplayEventInterface>(BlockPtr.Get());
		if (InterfaceObj)
		{
			// 이벤트 전송
			InterfaceObj->HandleGameplayEvent(Payload.EventTag, Payload);
		}

		// 기존 코드
		/*
		if (BlockPtr.IsValid())
		{
			BlockPtr->SetHighlightState(EBlockHighlightState::None);
		}
		*/
	}
	AffectedBlocks.Empty();
}

void UGA_AttackRange::RestoreMontageSpeed()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (Character && Character->GetMesh()->GetAnimInstance())
	{
		// n초가 지났으니 1배속으로 쾅!
		Character->GetMesh()->GetAnimInstance()->Montage_SetPlayRate(AttackMontage, 1.0f);
	}
}