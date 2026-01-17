// Fill out your copyright notice in the Description page of Project Settings.

#include "Object/Explosive.h"
#include "BlockInfoInterface.h"
#include "GameplayEventInterface.h"
#include "BlockGameplayTags.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"           
#include "AbilitySystemComponent.h" 
#include "AbilitySystemInterface.h" 
#include "Engine/OverlapResult.h"   
#include "DrawDebugHelpers.h"

// Sets default values
AExplosive::AExplosive()
{
	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // Initialize 호출 전까지 Tick 비활성화

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	/*
	* 폭발물은 블록위에 붙은 표시일 뿐 다른 객체와 상호작용하지 않음
	* 물리엔진 충돌 끔
	*/
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));
}

void AExplosive::BeginPlay()
{
	Super::BeginPlay();
}

void AExplosive::Initialize(
	FVector StartLoc,
	AActor* Target,
	float FlightDuration,
	float InAutoDetonateDelay,
	float InExplosionRadius,
	int32 InMaxBombCount,
	UAbilitySystemComponent* InSourceASC,
	FGameplayEffectSpecHandle InDamageSpecHandle,
	TSubclassOf<UGameplayEffect> InDestructionEffectClass)
{
	StartLocation = StartLoc;
	TargetBlock = Target;
	TotalFlightTime = FlightDuration;
	CurrentFlightTime = 0.0f;

	AutoDetonateDelay = InAutoDetonateDelay;
	ExplosionRadius = InExplosionRadius;
	MaxBombCount = InMaxBombCount;
	SourceASC = InSourceASC;
	DamageSpecHandle = InDamageSpecHandle;
	DestructionEffectClass = InDestructionEffectClass;

	if (TargetBlock.IsValid())
	{
		// 인터페이스를 통한 그리드 정보 접근
		if (IBlockInfoInterface* BlockInfo = Cast<IBlockInfoInterface>(TargetBlock.Get()))
		{
			// 목표 지점: 블록 중심 + Z (그리드 절반)
			float HalfGridSize = BlockInfo->GetBlockGridSize() / 2.0f;
			FVector BlockLoc = BlockInfo->GetBlockLocation();
			TargetLocation = BlockLoc + FVector(0.0f, 0.0f, HalfGridSize);
		}
		else
		{
			// 인터페이스가 없으면 그냥 액터 위치 사용 (기본값)
			TargetLocation = TargetBlock.Get()->GetActorLocation();
		}

		SetActorLocation(StartLocation);
		SetActorTickEnabled(true);

		// 블록이 파괴되는지 감시 (블록이 파괴될 때 같이 터지기 위해)
		TargetBlock->OnDestroyed.AddDynamic(this, &AExplosive::OnBlockDestroyed);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AExplosive::Initialize: TargetActor is null"));
		Destroy();
	}
}

void AExplosive::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bAttached) return;

	CurrentFlightTime += DeltaTime;

	// 진행률 (0.0 ~ 1.0)
	float Alpha = FMath::Clamp(CurrentFlightTime / TotalFlightTime, 0.0f, 1.0f);

	// 선형 보간 (수평 이동)
	FVector LerpLocation = FMath::Lerp(StartLocation, TargetLocation, Alpha);

	// 포물선 높이 추가 (4 * Height * x * (1-x))
	float HeightOffset = ArcHeight * 4.0f * Alpha * (1.0f - Alpha);

	FVector NewLocation = LerpLocation;
	NewLocation.Z += HeightOffset;

	SetActorLocation(NewLocation);

	// 도착 확인
	if (Alpha >= 1.0f)
	{
		OnLanded();
	}
}

void AExplosive::OnLanded()
{
	bAttached = true;
	SetActorTickEnabled(false);

	// 위치 보정
	SetActorLocation(TargetLocation);

	// 타겟 액터에 부착
	if (TargetBlock.IsValid())
	{
		// 목표 위치로 이동 (이미 위에서 보정했지만 안전장치)
		SetActorLocation(TargetLocation);

		// 블록에 부착 (위치 고정)
		FAttachmentTransformRules AttachmentRules(EAttachmentRule::KeepWorld, true);
		AttachToActor(TargetBlock.Get(), AttachmentRules);

		// 블록 색상 변경 요청 (인터페이스 & 태그 사용)
		// GA_StickyBomb의 Highlight Logic과 통일 (빨간색 = Attached)
		// 기존 코드의 "UpdateBombCount" 대신 태그 이벤트를 통해 상태 전달
		if (IGameplayEventInterface* EventInterface = Cast<IGameplayEventInterface>(TargetBlock.Get()))
		{
			FGameplayEventData Payload;
			Payload.EventTag = TAG_Block_Highlight_Target; // 혹은 폭탄 부착 전용 태그 사용 가능
			Payload.Instigator = this;
			// 폭탄 개수는 Payload의 Magnitude 등으로 전달하거나, 블록이 알아서 처리하도록 규약 필요
			// 여기서는 단순히 하이라이트 이벤트만 전송 (단순화)
			EventInterface->HandleGameplayEvent(TAG_Block_Highlight_Bomb, Payload);
		}
	}
	else
	{
		// 도착하고 보니 타겟 블록이 없어진 경우 즉시 기폭
		Detonate();
	}

	// 자동 폭파 타이머 시작
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DetonateTimerHandle, this, &AExplosive::OnAutoDetonate, AutoDetonateDelay, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AExplosive::OnLanded: World is null, cannot start timer"));
	}
}

void AExplosive::Detonate()
{
	// 블록 파괴 델리게이트 해제
	if (TargetBlock.IsValid())
	{
		TargetBlock->OnDestroyed.RemoveDynamic(this, &AExplosive::OnBlockDestroyed);

		// 블록 색상 복구 (이벤트 전송)
		if (IGameplayEventInterface* EventInterface = Cast<IGameplayEventInterface>(TargetBlock.Get()))
		{
			FGameplayEventData Payload;
			Payload.EventTag = TAG_Block_Highlight_None;
			EventInterface->HandleGameplayEvent(TAG_Block_Highlight_None, Payload);
		}
	}

	// 타이머 중지
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetonateTimerHandle);
	}

	// GA에게 폭발 사실 알림
	if (OnDetonatedDelegate.IsBound())
	{
		OnDetonatedDelegate.Broadcast();
	}

	// 폭발 범위 데미지 처리 
	FVector ExplosionCenter = GetActorLocation();

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (SourceASC.IsValid() && SourceASC->GetAvatarActor()) QueryParams.AddIgnoredActor(SourceASC->GetAvatarActor());

	bool bHit = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		ExplosionCenter,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(ExplosionRadius),
		QueryParams
	);

	if (bHit)
	{
		for (const FOverlapResult& Overlap : OverlapResults)
		{
			AActor* HitActor = Overlap.GetActor();
			if (!HitActor) continue;

			UAbilitySystemComponent* TargetASC = nullptr;
			if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(HitActor))
			{
				TargetASC = ASI->GetAbilitySystemComponent();
			}

			if (TargetASC && SourceASC.IsValid())
			{
				// 1. 데미지 적용
				if (DamageSpecHandle.IsValid())
				{
					SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
				}

				// 2. 파괴 효과 (Destruction)
				if (DestructionEffectClass)
				{
					FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
					Context.AddSourceObject(this);
					FGameplayEffectSpecHandle DestSpecHandle = SourceASC->MakeOutgoingSpec(DestructionEffectClass, 1.0f, Context);

					if (DestSpecHandle.IsValid())
					{
						SourceASC->ApplyGameplayEffectSpecToTarget(*DestSpecHandle.Data.Get(), TargetASC);
					}
				}
			}
		}
	}

	DrawDebugSphere(GetWorld(), ExplosionCenter, ExplosionRadius, 16, FColor::Red, false, 2.0f, 0, 2.0f);

	Destroy();
}

void AExplosive::OnAutoDetonate()
{
	Detonate();
}

void AExplosive::OnBlockDestroyed(AActor* DestroyedActor)
{
	// TargetBlock이 파괴되었으므로 WeakPtr이 자동으로 무효화됨 (IsValid 체크만 하면 됨)
	// 하지만 명시적으로 로직을 분기하기 위해 체크

	// 이미 부착된 상태라면 즉시 폭발
	if (bAttached)
	{
		Detonate();
	}
}