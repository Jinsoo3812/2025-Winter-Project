// Fill out your copyright notice in the Description page of Project Settings.


#include "Object/Explosive.h"
#include "Block/BlockBase.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	ABlockBase* Target,
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

	if (TargetBlock)
	{
		// 목표 지점: 블록 중심 + Z 50 (윗면 중앙)
		float HarfGridSize = TargetBlock->GetGridSize() / 2.0f;
		FVector BlockLoc = TargetBlock->GetActorLocation();
		TargetLocation = BlockLoc + FVector(0.0f, 0.0f, HarfGridSize);

		SetActorLocation(StartLocation);
		SetActorTickEnabled(true);

		// 블록이 파괴되는지 감시 (블록이 파괴될 때 같이 터지기 위해)
		TargetBlock->OnDestroyed.AddDynamic(this, &AExplosive::OnBlockDestroyed);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AExplosive::Initialize: TargetBlock is null"));
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

	// 바닥 블록 빨간색으로 변경
	if (TargetBlock)
	{
		// 목표 위치로 이동
		SetActorLocation(TargetLocation);

		// 블록에 부착 (위치 고정)
		FAttachmentTransformRules AttachmentRules(EAttachmentRule::KeepWorld, true);
		AttachToActor(TargetBlock, AttachmentRules);

		TargetBlock->UpdateBombCount(1, MaxBombCount);
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
	// 블록 파괴에 걸어둔 델리게이트 해제
	if (TargetBlock)
	{
		TargetBlock->OnDestroyed.RemoveDynamic(this, &AExplosive::OnBlockDestroyed);
	}

	// 타이머가 돌고 있다면 중지 (수동 기폭 시 중복 실행 방지)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetonateTimerHandle);
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("AExplosive::Detonate: World is null, cannot clear timer"));
	}

	// GA에게 폭발 사실 알림
	if (OnDetonatedDelegate.IsBound())
	{
		OnDetonatedDelegate.Broadcast();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("AExplosive::Detonate: No one is listening to OnDetonatedDelegate"));
	}

	// 폭발 범위 데미지 처리 
	FVector ExplosionCenter = GetActorLocation();

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	// 시전자(플레이어)는 피해를 입지 않도록 제외
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

			// ASC 확인
			UAbilitySystemComponent* TargetASC = nullptr;
			if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(HitActor))
			{
				TargetASC = ASI->GetAbilitySystemComponent();
			}

			if (TargetASC && SourceASC.IsValid())
			{
				// 1. 데미지 Effect 적용
				if (DamageSpecHandle.IsValid())
				{
					SourceASC->ApplyGameplayEffectSpecToTarget(
						*DamageSpecHandle.Data.Get(),
						TargetASC
					);
					UE_LOG(LogTemp, Log, TEXT("AExplosive::Detonate: Applied damage to %s"), *HitActor->GetName());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("AExplosive::Detonate: DamageSpecHandle is invalid"));
				}

				// 2. 파괴 Effect 적용
				if (DestructionEffectClass)
				{
					// Context 생성
					FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
					Context.AddSourceObject(this);

					// Spec 생성
					FGameplayEffectSpecHandle DestSpecHandle = SourceASC->MakeOutgoingSpec(
						DestructionEffectClass,
						1.0f, // Level
						Context
					);

					if (DestSpecHandle.IsValid())
					{
						SourceASC->ApplyGameplayEffectSpecToTarget(
							*DestSpecHandle.Data.Get(),
							TargetASC
						);
					}
				}
			}
		}
	}

	// 디버그 구 그리기
	DrawDebugSphere(GetWorld(), ExplosionCenter, ExplosionRadius, 16, FColor::Red, false, 2.0f, 0, 2.0f);

	// 블록 색상 복구
	if (TargetBlock)
	{
		TargetBlock->UpdateBombCount(-1, MaxBombCount);
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("AExplosive::Detonate: TargetBlock is invalid during detonation"));
	}

	// 폭발 처리
	Destroy();
}

// 타이머 콜백
void AExplosive::OnAutoDetonate()
{
	UE_LOG(LogTemp, Log, TEXT("AExplosive::OnAutoDetonate: Time is up!"));
	Detonate();
}

void AExplosive::SetBlockColorRed(bool bEnable)
{
	if (TargetBlock)
	{
		UStaticMeshComponent* BlockMesh = TargetBlock->GetBlockMesh();
		if (BlockMesh)
		{
			// 빨간색(2.0f) 또는 원래대로(0.0f) 설정
			// CPD Index 0 사용 (1=Preview, 2=Red/Attached, 3=Green/Targeted)
			float ColorValue = bEnable ? 2.0f : 0.0f;
			BlockMesh->SetCustomPrimitiveDataFloat(0, ColorValue);
		}
		else
		{
			// 유효성 검사 실패 시 로그 출력
			UE_LOG(LogTemp, Warning, TEXT("AExplosive::SetBlockColorRed: BlockMesh is null"));
		}
	}
	else
	{
		// 유효성 검사 실패 시 로그 출력
		UE_LOG(LogTemp, Warning, TEXT("AExplosive::SetBlockColorRed: TargetBlock is null"));
	}
}

void AExplosive::OnBlockDestroyed(AActor* DestroyedActor)
{
	// 블록은 이미 파괴 과정에 있으므로, 타겟 포인터를 null로 비워 폭발 로직에서 접근하지 못하게 함
	TargetBlock = nullptr;

	// 타겟 블록이 파괴되었다면?
	if (bAttached)
	{
		// 이미 부착된 상태라면: 블록과 함께 즉시 폭발해야 함
		Detonate();
	}
	else
	{
		// 날아가는 중이라면: 여기서 터뜨리지 않음
		// Tick은 계속 돌 것이고, OnLanded()에 도착했을 때 TargetBlock이 null이므로 그때 터짐
	}
}
