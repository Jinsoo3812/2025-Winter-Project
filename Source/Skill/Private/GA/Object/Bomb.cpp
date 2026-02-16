// Fill out your copyright notice in the Description page of Project Settings.


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
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "PhysicalSurfaces.h"
#include "BlockGameplayTags.h"
#include "DrawDebugHelpers.h"

ABomb::ABomb()
{
	PrimaryActorTick.bCanEverTick = false;

	// 충돌 컴포넌트 설정
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComp->OnComponentHit.AddDynamic(this, &ABomb::OnHit);
	CollisionComp->bReturnMaterialOnMove = true;
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

	// 시전자와 충돌하지 않도록 설정
	if (GetInstigator())
	{
		CollisionComp->IgnoreActorWhenMoving(GetInstigator(), true);
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

	FVector GravityDrop = FVector(0, 0, 0.5f * GravityZ * Time * Time);

	FVector Velocity = (Displacement - GravityDrop) / Time;

	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = Velocity;
	}
}

void ABomb::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!OtherActor || !OtherComp || !Hit.PhysMaterial.IsValid())
	{
		Detonate(); // 매터리얼이 없거나 유효하지 않으면 즉시 폭발
		return;
	}

	// Physical Material에서 SurfaceType 가져오기 (O(1) 비용)
	EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

	// Sticky 여부 판별 (Surface_Sticky가 Project Settings에서 SurfaceType1이라고 가정)
	if (SurfaceType == SURFACE_STICKY)
	{
		StickToTarget(OtherComp, NAME_None, Hit);
	}
	else
	{
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

	if (Payload.BlockSystem)
	{
		// BlockSystem에 구현된 함수를 사용하여 HitResult로부터 정확한 BlockReference 추출
		if (Payload.BlockSystem->GetBlockFromHitResult(Hit, AttachedBlockRef))
		{
			// 추출에 성공했다면 하이라이트 적용 (Payload에 HighlightTag가 있다고 가정)
			Payload.BlockSystem->HighlightBlock(AttachedBlockRef, Payload.BombHighlightTag);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Bomb: Hit surface is not a valid block for highlighting."));
		}
	}

	// 부착 이벤트 전송
	if (Payload.InstigatorASC.IsValid() && Payload.AttachedEventTag.IsValid())
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


	if (Payload.InstigatorASC.IsValid() && Payload.DetonationEventTag.IsValid())
	{
		FGameplayEventData EventData;
		EventData.Instigator = this;
		EventData.Target = this;

		// 추출한 피해자 명단을 TargetData에 포장
		if (ActorsToApplyGE.Num() > 0)
		{
			FGameplayAbilityTargetData_ActorArray* TargetDataActorArray = new FGameplayAbilityTargetData_ActorArray();
			for (AActor* TargetActor : ActorsToApplyGE)
			{
				TargetDataActorArray->TargetActorArray.Add(TargetActor);
			}
			EventData.TargetData.Add(TargetDataActorArray);
		}

		// 시전자에게 이벤트 발송
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Payload.InstigatorASC->GetAvatarActor(),
			Payload.DetonationEventTag,
			EventData
		);
	}

	Payload.BlockSystem->HighlightBlock(AttachedBlockRef, TAG_Block_Highlight_Bomb_None);

	Destroy();
}