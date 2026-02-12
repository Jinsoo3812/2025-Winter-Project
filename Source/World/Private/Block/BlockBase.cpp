// Fill out your copyright notice in the Description page of Project Settings.


#include "Block/BlockBase.h"
#include "Engine/World.h"
#include "CollisionChannels.h"
#include "BlockConfig.h"
#include "BlockGameplayTags.h"
#include "BlockSettings.h"
#include "ChunkBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"


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

void ABlockBase::InitializeBlock(const UBlockSpawnPayload* InPayload)
{
	// Payload를 사용하지 않는 블록을 위한 기본 구현
}

// Called when the game starts or when spawned
void ABlockBase::BeginPlay()
{
	Super::BeginPlay();

	// BlockConfig 캐시
	if (!BlockConfig)
	{
		if (const UBlockSettings* Settings = GetDefault<UBlockSettings>())
		{
			if (!Settings->BlockConfigAsset.IsNull())
			{
				// 디스크에서 에셋을 찾아 메모리에 올림
				// 로딩이 완료될 때까지 이곳에서 실행흐름이 멈춤
				BlockConfig = Settings->BlockConfigAsset.LoadSynchronous();
			}
		}
	}

	if (BlockConfig)
	{
		GridSize = BlockConfig->GridSize;

		// 1. 현재 내 클래스(this->GetClass())에 해당하는 태그를 Config에서 조회
		FGameplayTag MyBlockTag = BlockConfig->GetBlockDef(this->GetClass())->Tag;

		if (MyBlockTag.IsValid())
		{
			// 2. ASC 가져오기 (IAbilitySystemInterface를 구현했는지 확인)
			UAbilitySystemComponent* ASC = nullptr;

			// 인터페이스를 통한 접근 시도 (가장 권장되는 방식)
			if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(this))
			{
				ASC = ASI->GetAbilitySystemComponent();
			}
			// 인터페이스가 없다면 컴포넌트 검색 시도 (Fallback)
			else
			{
				ASC = FindComponentByClass<UAbilitySystemComponent>();
			}

			// 3. ASC가 유효하다면 Loose Tag 추가
			if (ASC)
			{
				// LooseGameplayTag: GameplayEffect 없이 ASC에 직접 태그를 추가함.
				// 블록의 타입(Identity)처럼 정적인 속성을 정의할 때 적합함.
				ASC->AddLooseGameplayTag(MyBlockTag);

				// (디버깅용 로그 - 필요 시 주석 해제)
				// UE_LOG(LogTemp, Log, TEXT("BlockBase: Assigned Tag %s to %s"), *MyBlockTag.ToString(), *GetName());
			}
			else
			{
				// BlockBase 자체는 ASC가 없을 수 있으므로(파괴 불가능 블록 등),
				// ASC가 없다고 해서 반드시 에러는 아님. 필요하다면 로그 출력.
				// UE_LOG(LogTemp, Warning, TEXT("BlockBase: No ASC found on %s, could not assign tag."), *GetName());
			}
		}
		else
		{
			// Config에는 있지만 현재 클래스와 매칭되는 정의가 없는 경우
			UE_LOG(LogTemp, Warning, TEXT("BlockBase: No matching ActorTag found in BlockConfig for Class %s"), *GetClass()->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BlockBase: BlockConfig failed to load in %s"), *GetName());
	}
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

	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockBase: MeshComponent not found in %s"), *GetName());
	}
}

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

	// 레이캐스트 길이를 속도에 비례하게 늘려서, 고속 낙하 시 터널링 방지 (60부터 시작)
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
			bHitSomething = false;
		}
	}

	if (bHitSomething)
	{
		// RayCast 충돌 표면 + HalfGridSize 위치로 스냅
		float CorrectedZ = HitResult.ImpactPoint.Z + (GridSize * 0.5f);

		FVector NewLocation = GetActorLocation();
		NewLocation.Z = CorrectedZ;

		if (SetActorLocation(NewLocation))
		{
			if (bIsFalling)
			{
				CheckLanding();
			}
			else
			{
				SetActorTickEnabled(false);
			}

			bIsFalling = false;
			VerticalVelocity = 0.0f;
		}
	}
	else
	{
		// 바닥이 없음 -> 낙하 시작 처리
		if (!bIsFalling)
		{
			bIsFalling = true;
			NotifyUpperBlock(); // 위의 블록 깨우기

			// 낙하 시작 시 청크 데이터 비우기 (Start Falling)
			if (ParentChunk.IsValid())
			{
				// 청크 로컬 좌표계
				FVector LocalPos = GetActorLocation() - ParentChunk->GetActorLocation();

				int32 X = FMath::RoundToInt(LocalPos.X / GridSize);
				int32 Y = FMath::RoundToInt(LocalPos.Y / GridSize);
				int32 Z = FMath::RoundToInt(LocalPos.Z / GridSize);

				// 자신의 자리를 비움
				ParentChunk->SetBlockData(X, Y, Z, EBlockType::None, false);
			}
		}

		// 물리 처리
		VerticalVelocity += GravityAcceleration * DeltaTime;
		FVector DeltaMove = FVector(0.0f, 0.0f, VerticalVelocity * DeltaTime);
		AddActorWorldOffset(DeltaMove, false);
	}
}

void ABlockBase::CheckLanding()
{
	FVector CurrentLoc = GetActorLocation();

	float SnappedX = FMath::RoundToFloat(CurrentLoc.X / GridSize) * GridSize;
	float SnappedY = FMath::RoundToFloat(CurrentLoc.Y / GridSize) * GridSize;
	float SnappedZ = FMath::RoundToFloat(CurrentLoc.Z / GridSize) * GridSize;

	// 논리적 좌표 계산 (청크 데이터 갱신용)
	int32 TargetX = 0, TargetY = 0, TargetZ = 0;
	FVector SnappedWorldLoc(SnappedX, SnappedY, SnappedZ);

	if (ParentChunk.IsValid())
	{
		// 청크 로컬 좌표계
		FVector LocalLoc = SnappedWorldLoc - ParentChunk->GetActorLocation();
		TargetX = FMath::RoundToInt(LocalLoc.X / GridSize);
		TargetY = FMath::RoundToInt(LocalLoc.Y / GridSize);
		TargetZ = FMath::RoundToInt(LocalLoc.Z / GridSize);

		// 내가 착지하려는 곳에 갑자기 무언가 생긴 경우
		FBlockData ExistingData = ParentChunk->GetBlockData(TargetX, TargetY, TargetZ);
		if (ExistingData.Type != EBlockType::None)
		{
			// 이미 자리가 찼으므로 한 칸 위로 올림
			TargetZ += 1;
			SnappedZ += GridSize;
			UE_LOG(LogTemp, Warning, TEXT("BlockBase: Race condition! Stacking up to Z=%d"), TargetZ);
		}

		// 내 블록 타입 가져오기
		EBlockType MyType = EBlockType::Destructible; // 기본값
		if (BlockConfig)
		{
			// 캐시된 Config에서 내 클래스 정보 조회
			const FBlockDefinition* Def = BlockConfig->GetBlockDef(GetClass());
			if (Def) MyType = Def->Type;
		}

		// Chunk에 도착 후 자신 기록
		ParentChunk->SetBlockData(TargetX, TargetY, TargetZ, MyType, true);
	}

	// 최종 위치 적용
	FVector FinalLoc(SnappedX, SnappedY, SnappedZ);
	if (SetActorLocation(FinalLoc))
	{
		// 스냅 성공
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
		}
		else
		{
			// 블록이 아닌 다른 물체일 경우
		}
	}
	else
	{
		// 위에 아무것도 없으므로 아무 작업도 하지 않음
	}
}

void ABlockBase::HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData& Payload)
{
	if (!BlockConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockBase: BlockConfig is null in %s"), *GetName());
		return;
	}

	// 태그 분류 (Router)
	if (EventTag.MatchesTag(TAG_Block_Highlight))
	{
		HandleHighlight(EventTag, Payload);
	}
}

void ABlockBase::HandleHighlight(FGameplayTag EventTag, const FGameplayEventData& Payload)
{
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("BlockBase: MeshComponent is null during HandleHighlight in %s"), *GetName());
		return;
	}

	// 폭탄 하이라이트 특수 처리
	if (EventTag.MatchesTag(TAG_Block_Highlight_Bomb))
	{
		// 폭탄 해제
		if (EventTag.MatchesTag(TAG_Block_Highlight_Bomb_None))
		{
			CurrentBombCount = 0;
			MeshComponent->SetCustomPrimitiveDataFloat(BlockConfig->BombCPDIndex, 0.0f);
		}
		// 폭탄 카운트 증가
		else
		{
			// 최대 폭탄 개수에 맞춰 Clamp
			CurrentBombCount = FMath::Clamp(CurrentBombCount + 1, 0, MaxBombCount);

			// CPD 값 계산 (미리 설정된 강도 * 폭탄 개수)
			float NewValue = CurrentBombCount * BlockConfig->BombIntensityPerCount;

			MeshComponent->SetCustomPrimitiveDataFloat(BlockConfig->BombCPDIndex, NewValue);
		}
		return;
	}

	// 일반적인 On/Off 형태의 Highlight 태그 처리
	if (const FBlockCPDInfo* FoundInfo = BlockConfig->HighlightSettings.Find(EventTag))
	{
		// 찾은 정보대로 CPD 업데이트
		MeshComponent->SetCustomPrimitiveDataFloat(FoundInfo->CPDIndex, FoundInfo->CPDValue);
	}
	else
	{
		// Highlight 태그인 줄 알고 들어왔는데, Config에도 없고 Bomb도 아닌 경우
		UE_LOG(LogTemp, Warning, TEXT("BlockBase: Undefined Highlight Tag %s in Config"), *EventTag.ToString());
	}
}

void ABlockBase::SelfDestroy()
{	
	// 죽기 전에 청크에게 내 자리 비워달라고 요청
	if (ParentChunk.IsValid())
	{
		// 현재 나의 월드 좌표를 넘겨줌 (청크가 알아서 로컬 좌표로 변환할 것임)
		ParentChunk->RemoveBlockAtWorldLocation(GetActorLocation());
	}
	else UE_LOG(LogTemp, Warning, TEXT("[BlockBase] I (%s) am destroyed, but my ParentChunk is invalid."), *GetName());

	Destroy();
}

void ABlockBase::SetParentChunk(AChunkBase* InChunk)
{
	ParentChunk = InChunk;
}
