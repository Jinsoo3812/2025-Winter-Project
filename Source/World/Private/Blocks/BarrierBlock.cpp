// Fill out your copyright notice in the Description page of Project Settings.


#include "BarrierBlock.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "ChunkBase.h"

ABarrierBlock::ABarrierBlock()
{
	// 투사체 컴포넌트 생성
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));

	// 초기에는 발사되지 않으므로 비활성화
	ProjectileMovement->SetUpdatedComponent(GetRootComponent());
	ProjectileMovement->bAutoActivate = false;

	// 중력 영향 없이 직사
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 0.0f;

	// 충돌 시 튕기지 않고 멈춤 (OnHit에서 직접 처리하므로 false 추천)
	ProjectileMovement->bShouldBounce = false;
}

void ABarrierBlock::InitializeBlock(const UBlockSpawnPayload* InPayload)
{
	Super::InitializeBlock(InPayload);

	if (InPayload)
	{
		if (const UBarrierSpawnPayload* BarrierData = Cast<UBarrierSpawnPayload>(InPayload))
		{
			this->LaunchSpeed = BarrierData->LaunchSpeed;
			this->MaxLifeTime = BarrierData->MaxLifeTime;
			this->TeamAllyTag = BarrierData->TeamAllyTag;
			this->TeamEnemyTag = BarrierData->TeamEnemyTag;
			this->AllyKnockbackStrength = BarrierData->AllyKnockbackStrength;
			this->DamageEffectClass = BarrierData->DamageEffectClass;
		}
	}
}

void ABarrierBlock::BeginPlay()
{
	Super::BeginPlay();

	// 충돌 이벤트 바인딩
	if (CollisionComponent)
	{
		// BlockBase에서 CollisionComponent는 RootComponent입니다.
		CollisionComponent->OnComponentHit.AddDynamic(this, &ABarrierBlock::OnHit);

		// 발사 시 충돌 처리를 위해 설정 변경 (QueryAndPhysics)
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else UE_LOG(LogTemp, Error, TEXT("BarrierBlock: CollisionComponent is null in %s"), *GetName());
}

void ABarrierBlock::Launch(FVector Direction)
{
	if (!ProjectileMovement)
	{
		UE_LOG(LogTemp, Error, TEXT("BarrierBlock: ProjectileMovement is null in %s"), *GetName());
		return;
	}

	// 자폭 타이머 시작
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(LifeTimerHandle, this, &ABarrierBlock::Explode, MaxLifeTime, false);
	}

	// 부모 청크에서 자신을 제거하여 청크 관리에서 제외
	if (ParentChunk.IsValid())
	{
		ParentChunk->RemoveBlockAtWorldLocation(GetActorLocation());
		ParentChunk = nullptr;
	}

	// 컴포넌트 활성화 및 속도 설정
	ProjectileMovement->Activate();
	ProjectileMovement->Velocity = Direction * LaunchSpeed;
}

void ABarrierBlock::OnHit(UPrimitiveComponent* HitComp,
	AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,const FHitResult& Hit)
{
	// 발사되지 않은 상태(정지 상태)에서의 충돌은 부모(파괴 가능 벽) 로직을 따르도록
	if (!ProjectileMovement || !ProjectileMovement->IsActive())
	{
		return;
	}

	// 자기 자신 무시
	if (!OtherActor || OtherActor == this) return;

	// 동료 배리어 블록 무시
	if (OtherActor && OtherActor != this)
	{
		// 상대방도 배리어 블록이고, 시전자가 같다면?
		if (OtherActor->IsA(ABarrierBlock::StaticClass()) && GetInstigator() == OtherActor->GetInstigator())
		{
			return; // 그냥 통과 (폭발 X, 넉백 X)
		}
	}

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OtherActor);
	UAbilitySystemComponent* TargetASC = ASI ? ASI->GetAbilitySystemComponent() : OtherActor->FindComponentByClass<UAbilitySystemComponent>();

	if (TargetASC)
	{
		// 생명체(GAS 보유) 분기

		// A. 아군 (밀어내기)
		if (TargetASC->HasMatchingGameplayTag(TeamAllyTag))
		{
			if (ACharacter* TargetChar = Cast<ACharacter>(OtherActor))
			{
				FVector KnockbackDir = ProjectileMovement->Velocity.GetSafeNormal();
				// Z축 살짝 추가하여 띄우기
				KnockbackDir.Z = 0.2f;

				TargetChar->LaunchCharacter(KnockbackDir * AllyKnockbackStrength, true, true);

				UE_LOG(LogTemp, Log, TEXT("BarrierBlock: Knockback Ally %s"), *OtherActor->GetName());

				// 아군을 튕겨낸 후 폭발
				Explode();
			}
		}
		// B. 적군 (데미지 & 폭발)
		else if (TargetASC->HasMatchingGameplayTag(TeamEnemyTag))
		{
			// GE 적용 (Damage)
			if (DamageEffectClass && GetAbilitySystemComponent())
			{
				FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
				ContextHandle.AddSourceObject(this);

				FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponent()->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);
				if (SpecHandle.IsValid())
				{
					GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
				}
			}

			UE_LOG(LogTemp, Log, TEXT("BarrierBlock: Hit Enemy %s -> Explode"), *OtherActor->GetName());
			Explode();
		}
		else
		{
			// GAS를 가졌지만 지정된 TAG가 없는 경우 -> 무조건 폭발
			Explode();
		}
	}
	else
	{
		// GAS가 없는 경우 (Terrain HISM 등) -> 무조건 폭발
		UE_LOG(LogTemp, Log, TEXT("BarrierBlock: Hit Terrain/Object %s -> Explode"), *OtherActor->GetName());
		Explode();
	}
}

void ABarrierBlock::Explode()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LifeTimerHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("BarrierBlock: Exploding %s"), *GetName());
	Destroy();
}