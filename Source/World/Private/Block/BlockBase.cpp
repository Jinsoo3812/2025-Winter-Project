// Fill out your copyright notice in the Description page of Project Settings.


#include "Block/BlockBase.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Type_Terrain, "Block.Type.Terrain");
UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Type_Destructible, "Block.Type.Destructible");

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

    // 충돌 프로필 설정 (기존 Mesh가 하던 역할)
    CollisionComponent->SetCollisionProfileName(TEXT("BlockAll"));
}

// Called when the game starts or when spawned
void ABlockBase::BeginPlay()
{
	Super::BeginPlay();
}

void ABlockBase::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (!MeshComponent)
    {
        // 모든 StaticMeshComponent를 다 가져온다.
        TArray<UStaticMeshComponent*> Components;
        GetComponents<UStaticMeshComponent>(Components);

        // 이름이 "Cube"인 것을 찾는다.
        for (UStaticMeshComponent* Comp : Components)
        {
            // 블루프린트에서 만든 컴포넌트 이름이 "Cube"인지 확인
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
        UE_LOG(LogTemp, Error, TEXT("ABlockBase: 이름이 'Cube'인 StaticMeshComponent를 찾을 수 없습니다."));
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

void ABlockBase::SpawnBlock(FVector SpawnLocation, EBlockType NewBlockType)
{
	Location = SpawnLocation;
	BlockType = NewBlockType;
	SetActorLocation(Location);

    // 블록이 소환되자마자 떨어져야 하는지 검사하기 위해 Tick 켬
	SetActorTickEnabled(true);
}

ABlockBase* ABlockBase::SpawnBlock(
	UWorld* World,
	TSubclassOf<ABlockBase> BlockClass,
	const FVector& SpawnLocation,
	bool bEnableGravity)
{
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("BlockBase::SpawnBlockAdvanced - World is null"));
		return nullptr;
	}

	if (!BlockClass)
	{
		UE_LOG(LogTemp, Error, TEXT("BlockBase::SpawnBlockAdvanced - BlockClass is null"));
		return nullptr;
	}

	// 기본 GridSize 가져오기 (CDO 사용)
	ABlockBase* CDO = BlockClass->GetDefaultObject<ABlockBase>();
	float GridSize = CDO ? CDO->GetGridSize() : 100.0f;

	// 점유 확인
	if (IsLocationOccupied(World, SpawnLocation, GridSize))
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockBase::SpawnBlockAdvanced - Location %s is occupied"), *SpawnLocation.ToString());
		return nullptr;
	}

	// 블록 생성
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABlockBase* NewBlock = World->SpawnActor<ABlockBase>(BlockClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	if (!NewBlock)
	{
		UE_LOG(LogTemp, Error, TEXT("BlockBase::SpawnBlockAdvanced - Failed to spawn block at %s"), *SpawnLocation.ToString());
		return nullptr;
	}

	// 블록 위치 설정
	NewBlock->Location = SpawnLocation;
	NewBlock->SetActorLocation(SpawnLocation);

	// 중력 설정
	if (bEnableGravity)
	{
		NewBlock->bCanFall = true;
		NewBlock->SetActorTickEnabled(true);
	}
	else
	{
		NewBlock->bCanFall = false;
		NewBlock->SetActorTickEnabled(false);
	}

	return NewBlock;
}

bool ABlockBase::IsLocationOccupied(
	UWorld* World,
	const FVector& CheckLocation,
	float CheckGridSize)
{
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("BlockBase::IsLocationOccupied - World is null"));
		return true;
	}

	// MakeBox는 인자를 반지름으로 사용함
	// 0.5를 넣으면 100 * 100 * 100 크기의 박스가 되어 꽉 차므로 0.4 사용
	FVector BoxExtent = FVector(CheckGridSize * 0.4f, CheckGridSize * 0.4f, CheckGridSize * 0.4f);
	FCollisionShape CheckShape = FCollisionShape::MakeBox(BoxExtent);

	// ObjectType 기반 쿼리 (WorldStatic, WorldDynamic만 체크)
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams;

	// 충돌한 블록들을 반환할 것이 아니므로 OverlapMulti 대신 OverlapAny 사용
	return World->OverlapAnyTestByObjectType(CheckLocation, FQuat::Identity, ObjectQueryParams, CheckShape, QueryParams);
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

// (참고) SetHighlightState는 Index 0만 건드리므로 폭탄 색(Index 1)에 영향 없음
void ABlockBase::SetHighlightState(EBlockHighlightState NewState)
{
    if (UStaticMeshComponent* Mesh = GetBlockMesh())
    {
        // Enum을 float로 변환하여 전달
        float StateValue = static_cast<float>(NewState);
        Mesh->SetCustomPrimitiveDataFloat(CPD_INDEX_HIGHLIGHT, StateValue);
    }
}

void ABlockBase::UpdateBombCount(int32 Delta, int32 MaxBombCount)
{
    CurrentBombCount = FMath::Clamp(CurrentBombCount + Delta, 0, MaxBombCount);

    if (UStaticMeshComponent* Mesh = GetBlockMesh())
    {
        // 0 ~ 1 사이 실수로 변환하여 전달 (예: 1개=0.33, 2개=0.66, 3개=1.0)
        float ColorRatio = (float)CurrentBombCount / (float)MaxBombCount;
        Mesh->SetCustomPrimitiveDataFloat(CPD_INDEX_BOMBCOUNT, ColorRatio); // Index 1 사용
    }
}

void ABlockBase::HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData& Payload)
{
    // 태그 확인: 하이라이트 이벤트인가?
    if (EventTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Event.Block.Highlight"))))
    {
        // Payload에서 상태값 추출 (GA에서 Magnitude에 Enum 값을 넣어서 보냈음)
        float StateValue = Payload.EventMagnitude;

        // 형변환 후 적용
        EBlockHighlightState NewState = static_cast<EBlockHighlightState>((int32)StateValue);

        // 기존 함수 재사용
        SetHighlightState(NewState);
    }
    else
    {
        // 처리할 수 없는 태그일 경우 로그
        UE_LOG(LogTemp, Warning, TEXT("ABlockBase::HandleGameplayEventInterface: Unhandled Tag %s"), *EventTag.ToString());
    }
}
