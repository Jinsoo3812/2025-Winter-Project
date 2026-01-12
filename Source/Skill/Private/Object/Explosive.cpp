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
		SetBlockColorRed(true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AExplosive::OnLanded: TargetBlock is invalid"));
	}

	// 자동 폭파 타이머 시작
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DetonateTimerHandle, this, &AExplosive::OnAutoDetonate, AutoDetonateDelay, false);
		UE_LOG(LogTemp, Log, TEXT("AExplosive::OnLanded: Timer started for %f seconds"), AutoDetonateDelay);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AExplosive::OnLanded: World is null, cannot start timer"));
	}

	// GA에게 착륙 사실 알림
	if (OnLandedDelegate.IsBound())
	{
		OnLandedDelegate.Broadcast();
	}
}

void AExplosive::Detonate()
{
	// 타이머가 돌고 있다면 중지 (수동 기폭 시 중복 실행 방지)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetonateTimerHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("AExplosive::Detonate: BOOM!"));

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
		SetBlockColorRed(false);
	}
	else
	{
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
	if (!TargetBlock) {
		UE_LOG(LogTemp, Warning, TEXT("AExplosive::SetBlockColorRed: TargetBlock is null"));
		return;
	}

	UStaticMeshComponent* BlockMesh = TargetBlock->GetBlockMesh();
	if (!BlockMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("AExplosive::SetBlockColorRed: BlockMesh is null"));
		return;
	}

	// 0번 슬롯 머티리얼 제어
	UMaterialInterface* CurrentMat = BlockMesh->GetMaterial(0);
	if (!CurrentMat) {
		UE_LOG(LogTemp, Warning, TEXT("AExplosive::SetBlockColorRed: CurrentMat is null"));
		return;
	}

	if (bEnable)
	{
		// 이미 Dynamic Material인지 확인하고 아니면 생성
		UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(CurrentMat);
		if (!DynMat)
		{
			DynMat = UMaterialInstanceDynamic::Create(CurrentMat, this);
			BlockMesh->SetMaterial(0, DynMat);
		}

		if (DynMat)
		{
			// 빨간색 하이라이트 (EmissiveColor 벡터 파라미터가 있다고 가정)
			// 만약 머티리얼에 해당 파라미터가 없다면 에디터에서 추가 필요
			DynMat->SetVectorParameterValue(FName("EmissiveColor"), FLinearColor::Red);
			DynMat->SetScalarParameterValue(FName("EmissivePower"), 10.0f);
		}
	}
	else
	{
		// 복구 로직: EmissivePower를 0으로 낮추거나, 파라미터를 초기화
		UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(CurrentMat);
		if (DynMat)
		{
			// 색상 및 파워 초기화 (가정된 기본값)
			DynMat->SetVectorParameterValue(FName("EmissiveColor"), FLinearColor::Black);
			DynMat->SetScalarParameterValue(FName("EmissivePower"), 0.0f);
		}
	}
}
