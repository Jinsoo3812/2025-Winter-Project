#include "AttackHitCheck.h"
#include "EnemyBase.h" // AEnemyBase의 OnAttackHit 함수 호출을 위해
#include "Engine/OverlapResult.h"
#include "Kismet/KismetSystemLibrary.h"

UAnimNotifyState_AttackHitCheck::UAnimNotifyState_AttackHitCheck()
{
	// 기본값 설정
	SocketName = FName("AttackSocket");
	AttackRadius = 50.0f;
	bDrawDebug = true;
}

void UAnimNotifyState_AttackHitCheck::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// 새로운 공격이 시작되었으므로, 타격했던 액터 목록을 비웁니다.
	HitActors.Empty();
}

void UAnimNotifyState_AttackHitCheck::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	// 1. 소켓의 현재 위치를 가져옵니다.
	FVector SocketLocation = MeshComp->GetSocketLocation(SocketName);

	// 2. 판정 수행 (Sphere Trace)
	// Pawn 채널(캐릭터)만 감지하도록 설정합니다. 필요에 따라 변경 가능합니다.
	TArray<FHitResult> HitResults;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(MeshComp->GetOwner()); // 자기 자신은 때리면 안 됨

	// * 핵심: 단순히 점을 찍는 게 아니라, 이전 프레임 위치를 기억해서 Sweep 하면 좋지만,
	//   여기서는 간단하게 매 프레임 구체를 생성하는 방식을 사용합니다. 
	//   (더 정교하게 하려면 NotifyBegin에서 LastLocation을 저장하고 Tick에서 SweepSingleByChannel 사용)
	bool bHit = UKismetSystemLibrary::SphereTraceMulti(
		MeshComp->GetWorld(),
		SocketLocation,
		SocketLocation, // 시작과 끝이 같으면 제자리 검사
		AttackRadius,
		UEngineTypes::ConvertToTraceType(ECC_Pawn), // 혹은 GameTraceChannel 사용
		false, // TraceComplex
		ActorsToIgnore,
		bDrawDebug ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None,
		HitResults,
		true // IgnoreSelf
	);

	if (bHit)
	{
		for (const FHitResult& Result : HitResults)
		{
			AActor* HitActor = Result.GetActor();
			// 이미 이번 공격에 맞은 애가 아니라면?
			if (HitActor && !HitActors.Contains(HitActor))
			{
				HitActors.Add(HitActor);

				// [중요] 공격 주체(EnemyBase)에게 "나 얘 때렸어!"라고 알립니다.
				// 이렇게 하면 데미지 계산 로직은 EnemyBase나 Ability에서 관리할 수 있습니다.
				if (AEnemyBase* Attacker = Cast<AEnemyBase>(MeshComp->GetOwner()))
				{
					Attacker->OnAttackHit(HitActor);
				}
			}
		}
	}
}

void UAnimNotifyState_AttackHitCheck::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	HitActors.Empty();
}