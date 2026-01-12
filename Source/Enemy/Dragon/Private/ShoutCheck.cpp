#include "ShoutCheck.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

UShoutCheck::UShoutCheck()
{
	SocketName = FName("MouthSocket");
	ShoutRadius = 1500.0f; // 꽤 넓은 범위
	ShoutConeAngle = 35.0f; // 전방 90도
	bDrawDebug = true;
}

void UShoutCheck::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	HitActors.Empty(); // 초기화
}

void UShoutCheck::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	UWorld* World = MeshComp->GetWorld();

	// 1. 기준 위치(입)와 방향 계산
	FVector MouthLocation = MeshComp->GetSocketLocation(SocketName);
	FVector MouthForward = MeshComp->GetSocketRotation(SocketName).Vector();

	// 높이 무시를 위해 Z값 제거 및 정규화(Normalize)
	MouthForward.Z = 0.0f;
	MouthForward.Normalize();

	// 2. 플레이어 찾기 (보통 PlayerController 0번이 플레이어)
	// 멀티플레이라면 OverlapSphere 등으로 모든 Pawn을 찾아야 하지만, 싱글이라면 이게 빠릅니다.
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!PlayerCharacter || HitActors.Contains(PlayerCharacter)) return;

	// 3. 판정 로직 (수학)
	FVector PlayerLocation = PlayerCharacter->GetActorLocation();
	FVector DirToPlayer = PlayerLocation - MouthLocation;

	// 높이 무시 (2D 거리 계산)
	DirToPlayer.Z = 0.0f;
	float DistanceSq = DirToPlayer.SizeSquared(); // 제곱 거리 (루트 연산 최적화)

	// (1) 거리 검사
	if (DistanceSq <= (ShoutRadius * ShoutRadius))
	{
		DirToPlayer.Normalize(); // 방향 벡터화

		// (2) 각도 검사 (내적 Dot Product)
		// 내적 값 1.0 = 0도(정면), 0.0 = 90도, -1.0 = 180도
		float DotResult = FVector::DotProduct(MouthForward, DirToPlayer);

		// 각도를 라디안으로 변환하여 코사인 값 비교
		// 예: 90도 범위라면 좌우 45도이므로, HalfAngle = 45도
		float HalfAngleRad = FMath::DegreesToRadians(ShoutConeAngle * 0.5f);
		float Threshold = FMath::Cos(HalfAngleRad);

		if (DotResult >= Threshold)
		{
			// [적중!] 조건 만족
			HitActors.Add(PlayerCharacter);

			// 4. 디버프(GE) 적용
			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerCharacter);
			if (TargetASC && DebuffEffect)
			{
				FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
				ContextHandle.AddSourceObject(OwnerActor);

				// 레벨 1.0으로 적용
				FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(DebuffEffect, 1.0f, ContextHandle);
				if (SpecHandle.IsValid())
				{
					TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
					UE_LOG(LogTemp, Warning, TEXT("Shout Hit! Player Slowed!"));
				}
			}
		}
	}

	// 5. 디버그 드로잉 (범위 눈으로 확인)
	if (bDrawDebug)
	{
		// 부채꼴 그리기
		FVector Center = MouthLocation;
		Center.Z = PlayerLocation.Z; // 플레이어 높이에 맞춰서 그림 (보기 편하게)

		UKismetSystemLibrary::DrawDebugCone(
			World, Center, MouthForward, ShoutRadius,
			FMath::DegreesToRadians(ShoutConeAngle * 0.5f), 0.0f, 12,
			FLinearColor::Red, 0.0f, 0.5f
		);
	}
}
