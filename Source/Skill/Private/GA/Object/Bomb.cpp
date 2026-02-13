#include "Bomb.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "TimerManager.h"
#include "BlockSystemInterface.h"
#include "CollisionChannels.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

ABomb::ABomb()
{
	PrimaryActorTick.bCanEverTick = false;

	// 충돌 컴포넌트 설정
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComp->OnComponentHit.AddDynamic(this, &ABomb::OnHit);
	RootComponent = CollisionComp;

	// 메시 컴포넌트 설정
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 투사체 이동 컴포넌트 설정
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.3f;
}

void ABomb::InitializeExplosive(const FBombPayload& InPayload)
{
	Payload = InPayload;

	if (!Payload.BlockSystem)
	{
		Payload.BlockSystem = IBlockSystemInterface::Get(this);
	}

	// Payload에 설정된 시간으로 '폭발' 타이머 설정
	if (Payload.AutoDetonateTime > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			DetonateTimerHandle,
			this,
			&ABomb::Detonate,
			Payload.AutoDetonateTime,
			false
		);
	}
}

void ABomb::LaunchByTime(FVector TargetLocation, float Time, float GravityZ)
{
	if (Time <= 0.0f) return;

	FVector StartLoc = GetActorLocation();
	FVector Displacement = TargetLocation - StartLoc;
	FVector GravityCorrection = FVector(0, 0, -0.5f * GravityZ * Time * Time);
	FVector Velocity = (Displacement - GravityCorrection) / Time;

	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = Velocity;
	}
}

void ABomb::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHasDetonated || !OtherActor) return;

	// 부착 가능 여부 판단 (태그)
	bool bIsSticky = false;

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);

	// ASC가 있고, 설정된 Sticky 태그가 있다면 확인
	if (TargetASC && StickyTargetTag.IsValid())
	{
		if (TargetASC->HasMatchingGameplayTag(StickyTargetTag))
		{
			bIsSticky = true;
		}
	}

	if (bIsSticky)
	{
		// HISM에도 붙게 하기 위해 Component에 부착
		StickToTarget(OtherComp, NAME_None, Hit);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Bomb: Hit non-sticky target. BOOM."));
		Detonate();
	}
}

void ABomb::StickToTarget(USceneComponent* TargetComp, FName SocketName, const FHitResult& Hit)
{
	if (!TargetComp) return;

	UE_LOG(LogTemp, Log, TEXT("Bomb: Sticking to Component %s"), *TargetComp->GetName());

	// 1. 물리 및 이동 정지
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->ProjectileGravityScale = 0.0f;
	}

	if (CollisionComp)
	{
		CollisionComp->SetSimulatePhysics(false);
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// HISM과 호환되기 위해 컴포넌트에 부착
	// HISM Instance 개별에 붙는 것이 아닌 컴포넌트와의 상대적 위치 유지
	FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, true);
	AttachToComponent(TargetComp, AttachRules, SocketName);

	// 부착 이벤트 전송
	if (Payload.InstigatorASC.IsValid() && Payload.DetonationEventTag.IsValid())
	{
		FGameplayEventData EventData;
		EventData.Instigator = this;
		EventData.Target = this;
		
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Payload.InstigatorASC->GetAvatarActor(),
			Payload.AttachedEventTag,
			EventData
		);
	}
}

void ABomb::Detonate()
{
	if (bHasDetonated) return;
	bHasDetonated = true;

	// 타이머가 돌고 있었다면 해제 (조기 폭발 시)
	GetWorld()->GetTimerManager().ClearTimer(DetonateTimerHandle);

	UE_LOG(LogTemp, Log, TEXT("Bomb: Detonating!"));

	// 디버그 및 시각 효과
	DrawDebugSphere(GetWorld(), GetActorLocation(), Payload.ExplosionRadius, 24, FColor::Red, false, 2.0f);

	// Enemy와 Block에 대해 Overlap 검사
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Enemy);
	ObjectParams.AddObjectTypesToQuery(ECC_Block);

	FCollisionShape SphereShape = FCollisionShape::MakeSphere(Payload.ExplosionRadius);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectParams, SphereShape, QueryParams);

	// GE를 맞을 최종 액터들(Set으로 중복 방지)
	TSet<AActor*> ActorsToApplyGE;;

	if (Payload.BlockSystem)
	{
		// Overlap에서 Actor Block만 추출
		TArray<FBlockReference> BlockRefs;
		Payload.BlockSystem->GetBlocksFromOverlaps(Overlaps, BlockRefs);

		for (const FBlockReference& Ref : BlockRefs)
		{
			if (!Ref.IsValid()) continue;

			// HISM 블록
			if (Ref.ItemIndex >= 0 && Ref.TargetComponent.IsValid())
			{
				// HSIM은 GAS가 없고, 애초에 파괴 가능하도록 설계되지가 않았으므로 넘김
			}
			// Actor 블록
			else if (AActor* BlockActor = Cast<AActor>(Ref.TargetObject.Get()))
			{
				// Actor 블록은 ASC가 있으므로 GE 대상에 추가
				ActorsToApplyGE.Add(BlockActor);
			}
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Bomb: BlockSystem is not valid. Skipping block processing."));
	}

	// Overlap에서 Enemy 추출
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlapActor = Overlap.GetActor();
		if (OverlapActor && !ActorsToApplyGE.Contains(OverlapActor))
		{
			// 블록이 아니면서 Pawn이나 Character인 경우
			if (OverlapActor->IsA<APawn>())
			{
				ActorsToApplyGE.Add(OverlapActor);
			}
		}
	}

	// GE Spec 적용
	if (ActorsToApplyGE.Num() > 0 && Payload.EffectSpecs.Num() > 0)
	{
		for (AActor* Target : ActorsToApplyGE)
		{
			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
			if (!TargetASC) continue;

			for (const FGameplayEffectSpecHandle& SpecHandle : Payload.EffectSpecs)
			{
				if (SpecHandle.IsValid())
				{
					TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				}
			}
		}
	}

	// 시전자에게 폭발 알림 이벤트 전송
	if (Payload.InstigatorASC.IsValid() && Payload.DetonationEventTag.IsValid())
	{
		FGameplayEventData EventData;
		EventData.Instigator = this;
		EventData.Target = this;

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Payload.InstigatorASC->GetAvatarActor(),
			Payload.DetonationEventTag,
			EventData
		);
	}

	Destroy();
}