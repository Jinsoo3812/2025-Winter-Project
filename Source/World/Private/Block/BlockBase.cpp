// Fill out your copyright notice in the Description page of Project Settings.


#include "Block/BlockBase.h"
#include "Engine/World.h"
#include "CollisionChannels.h"
#include "Block/DA_BlockConfig.h"
#include "BlockGameplayTags.h"


// Sets default values
ABlockBase::ABlockBase()
{
	// Tick을 사용할 수는 있지만,
	PrimaryActorTick.bCanEverTick = true;

	// 처음에는 비활성화 상태로 시작
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 물리 충돌을 담당할 BoxComponent 생성 (Root)
	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionComponent;

	// 박스 크기 설정: 100의 절반인 50에서 아주 살짝 줄인 49.5로 설정
	// 시각적(Mesh)으로는 100으로 꽉 차 보이지만, 물리적으로는 1.0의 틈이 생겨 마찰/끼임 방지
	CollisionComponent->SetBoxExtent(FVector(49.5f, 49.5f, 49.5f));

	// 블록들의 충돌 채널인 ECC_Block
	CollisionComponent->SetCollisionObjectType(ECC_Block);
}

// Called when the game starts or when spawned
void ABlockBase::BeginPlay()
{
	Super::BeginPlay();
}

void ABlockBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// MeshComponent 캐시
	if (!MeshComponent)
	{
		// 모든 StaticMeshComponent를 다 가져온다.
		TArray<UStaticMeshComponent*> Components;
		GetComponents<UStaticMeshComponent>(Components);

		// 이름이 "Cube"인 것을 찾는다.
		for (UStaticMeshComponent* Comp : Components)
		{
			// MeshComponent에 캐시
			// (주의: 에디터에서 보이는 이름과 실제 변수명이 다를 수 있으나, 보통 일치함)
			if (Comp && Comp->GetName().Contains(TEXT("Cube")))
			{
				MeshComponent = Comp;
				break;
			}
		}
	}

	// 유효성 검사 (찾았는지 확인)
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockBase: MeshComponent not found in %s"), *GetName());
	}
}

// Called every frame
void ABlockBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 낙하 기능이 있는 블록만 처리
	if (bCanFall) {
		UpdateGravity(DeltaTime);
	}
}

void ABlockBase::UpdateGravity(float DeltaTime)
{
	FVector Start = GetActorLocation();

	// 레이캐스트 길이를 속도에 비례하게 늘려서, 고속 낙하 시 터널링(바닥 뚫음) 방지
	// 최소 길이는 60
	float CheckDistance = FMath::Max(60.0f, (FMath::Abs(VerticalVelocity) * DeltaTime) + 10.0f);
	FVector End = Start + FVector(0.0f, 0.0f, -CheckDistance);

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
		HitResult, Start, End, ECC_Visibility, Params
	);

	// 바닥에 뭔가가 닿았는데, 그게 '추락 중인 블록'이라면 바닥이 없는 것으로 간주해야 함
	if (bHitSomething)
	{
		ABlockBase* HitBlock = Cast<ABlockBase>(HitResult.GetActor());
		if (HitBlock && HitBlock->IsFalling())
		{
			// 아래 있는 블록이 같이 추락 중이라면, 바닥이 없는 것으로 간주
			bHitSomething = false;
		}
	}

	if (bHitSomething)
	{
		// 바닥이 확실히 있음
		if (bIsFalling)
		{
			CheckLanding();
		}
		else
		{
			// 안정적인 상태 유지 (물리를 켜놓으면 어디에 끼거나 진동하거나 하여튼,,)
			SetActorTickEnabled(false);
		}

		bIsFalling = false;
		VerticalVelocity = 0.0f;
	}
	else
	{
		// 바닥이 없음 -> 낙하 처리
		if (!bIsFalling)
		{
			bIsFalling = true;
			NotifyUpperBlock(); // 내 위의 블록도 깨움
			// UE_LOG(LogTemp, Warning, TEXT("BlockBase: %s started falling."), *GetName());
		}

		// 속도 갱신
		VerticalVelocity += GravityAcceleration * DeltaTime;
		FVector DeltaMove = FVector(0.0f, 0.0f, VerticalVelocity * DeltaTime);

		// 옆면 마찰 문제 해결
		// 매 프레임 아래로 떨어지는 것은 일종의 순간이동인데
		// 그 때마다 양 옆에 부딪힌다면 제대로 이동하지 못하므로 sweep을 끔
		AddActorWorldOffset(DeltaMove, false);
	}
}

void ABlockBase::CheckLanding()
{
	FVector CurrentLoc = GetActorLocation();

	// 152, 99 처럼 중간에 걸친 위치를 그리드에 스냅
	float HalfSize = GridSize / 2.0f;
	float SnappedZ = FMath::RoundToFloat((CurrentLoc.Z - HalfSize) / GridSize) * GridSize + HalfSize;
	float SnappedX = FMath::RoundToFloat(CurrentLoc.X / GridSize) * GridSize; // X, Y는 중심이 0 기준이면 그대로 둠
	float SnappedY = FMath::RoundToFloat(CurrentLoc.Y / GridSize) * GridSize;

	FVector NewLoc = FVector(SnappedX, SnappedY, SnappedZ);

	if (SetActorLocation(NewLoc))
	{
		// 스냅 성공 및 가상 물리 연산 종료
		bIsFalling = false;
		VerticalVelocity = 0.0f;
		SetActorTickEnabled(false);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockBase: Failed to snap to grid %s"), *GetName());
	}
}

void ABlockBase::NotifyUpperBlock()
{
	// 내 위치에서 위로 100 + 10(offset) 만큼 레이캐스트
	FVector Start = GetActorLocation();
	FVector End = Start + FVector(0.0f, 0.0f, 110.0f);

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// 위쪽에 블록이 있는지 검사.
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult, Start, End, ECC_Visibility, Params
	);

	if (bHit && HitResult.GetActor())
	{
		// 닿은 액터가 BlockBase인지 확인하고 깨움
		ABlockBase* UpperBlock = Cast<ABlockBase>(HitResult.GetActor());
		if (UpperBlock)
		{
			// 잠자던 위쪽 블록의 Tick을 켜서 추락하게 함
			UpperBlock->SetActorTickEnabled(true);
			// 로그 확인용
			// UE_LOG(LogTemp, Log, TEXT("BlockBase: %s woke up %s"), *GetName(), *UpperBlock->GetName());
		}
		else
		{
			// 블록이 아닌 다른 물체일 경우 (로그 생략 가능)
		}
	}
	else
	{
		// 위에 아무것도 없으므로 아무 작업도 하지 않음 (정상 상황)
	}
}

void ABlockBase::HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData& Payload)
{
	// Config 유효성 체크
	if (!BlockConfig) {
		UE_LOG(LogTemp, Warning, TEXT("BlockBase: BlockConfig is null in %s"), *GetName());
		return;
	}

	// 폭탄 하이라이트 태그 처리는 중첩형이므로 특수 처리
	if (EventTag.MatchesTag(TAG_Block_Highlight_Bomb))
	{
		if (MeshComponent)
		{   
			if (EventTag.MatchesTag(TAG_Block_Highlight_Bomb_None))
			{
				CurrentBombCount = 0;
				MeshComponent->SetCustomPrimitiveDataFloat(BlockConfig->BombCPDIndex, 0.0f);
				return;
			}
			// 최대 폭탄 개수에 맞춰 Clamp
			CurrentBombCount = FMath::Clamp(CurrentBombCount + 1, 0, MaxBombCount);

			// CPD 값 계산 (미리 설정된 강도 * 폭탄 개수)
			float NewValue = CurrentBombCount * BlockConfig->BombIntensityPerCount;

			// 폭탄 CPD Index도 Config에 정의되어 있음
			MeshComponent->SetCustomPrimitiveDataFloat(BlockConfig->BombCPDIndex, NewValue);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BlockBase: MeshComponent is null during Bomb Event in %s"), *GetName());
		}
		return;
	}

	// 일반적인 On/Off 형태의 Highlight 태그 처리
	// Ex) Block.Highlight.Preview 같은 태그가 왔을 때,
	// BlockCPDIndexMap에서 해당 태그에 맞는 CPD 정보를 찾음
	if (const FBlockCPDInfo* FoundInfo = BlockConfig->BlockCPDIndexMap.Find(EventTag))
	{
		// 찾은 정보대로 CPD 업데이트
		if (MeshComponent)
		{
			MeshComponent->SetCustomPrimitiveDataFloat(FoundInfo->CPDIndex, FoundInfo->CPDValue);
		}
	}
	else
	{
		// 맵에도 없고, 특수 처리 태그도 아님 -> 경고
		// UE_LOG(LogTemp, Warning, TEXT("BlockBase: EventTag %s not found in BlockCPDIndexMap of %s"), *EventTag.ToString(), *GetName());
	}
}

void ABlockBase::HandleBombEvent(const FGameplayTag& EventTag)
{
	if (MeshComponent)
	{	
		// 폭탄이 모두 터져서 하이라이트를 제거해야 하는 경우
		if (EventTag.MatchesTag(TAG_Block_Highlight_Bomb_None))
		{
			CurrentBombCount = 0;
			MeshComponent->SetCustomPrimitiveDataFloat(BlockConfig->BombCPDIndex, 0.0f);
			return;
		}

		// 폭탄 개수 증가 하이라이트
		// 최대 폭탄 개수에 맞춰 Clamp
		CurrentBombCount = FMath::Clamp(CurrentBombCount + 1, 0, MaxBombCount);

		// CPD 값 계산 (미리 설정된 강도 * 폭탄 개수)
		float NewValue = CurrentBombCount * BlockConfig->BombIntensityPerCount;

		// 폭탄 CPD Index도 Config에 정의되어 있음
		MeshComponent->SetCustomPrimitiveDataFloat(BlockConfig->BombCPDIndex, NewValue);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockBase: MeshComponent is null during Bomb Event in %s"), *GetName());
	}
	return;
}

FVector ABlockBase::GetBlockAlignedLocation() const
{
	// 현재 실제 액터의 위치
	FVector CurrentLoc = GetActorLocation();

	float HalfSize = GridSize / 2.0f;

	// 0, 100, 200, -100... 등 정수 배수 좌표로 스냅
	float SnappedX = FMath::RoundToFloat(CurrentLoc.X / GridSize) * GridSize;
	float SnappedY = FMath::RoundToFloat(CurrentLoc.Y / GridSize) * GridSize;
	float SnappedZ = FMath::RoundToFloat(CurrentLoc.Z / GridSize) * GridSize;

	return FVector(SnappedX, SnappedY, SnappedZ);
}

void ABlockBase::SelfDestroy()
{
	Destroy();
}
