// BossDragon.cpp

#include "BossDragon.h"

// ---------------------------------------------------------------------------
// [1] 필수 헤더 포함
// ---------------------------------------------------------------------------
// 엔진 핵심 기능 및 컴포넌트
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h" // OverlapResult 및 충돌 처리용
#include "MotionWarpingComponent.h"        // 애니메이션 워핑용
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"          // GetActor() 등의 결과 처리를 위해 필수
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h" // 블랙보드 접근용

// GAS (Gameplay Ability System) 관련 헤더
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffectExtension.h"       // FOnAttributeChangeData 사용을 위해 필수

// 프로젝트 내부 모듈 헤더
#include "Enemy/Public/EnemyAttributeSet.h" 
#include "BlockBase.h"
#include "BlockSystemInterface.h"        // 블록 시스템 인터페이스 (0209 수정)
#include "BlockSpawnPayload.h"
#include "BlockManagerSubsystem.h"   // 블록 생성 관리자
#include "BlockGameplayTags.h"             // 블록 태그 정의 헤더
#include "CollisionChannels.h"

// ---------------------------------------------------------------------------
// [2] 생성자 (Constructor)
// ---------------------------------------------------------------------------
ABossDragon::ABossDragon()
{
	// 1. 돌진 공격용 히트박스(Collision) 설정
	RushHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RushHitBox"));
	RushHitBox->SetupAttachment(GetMesh());   // 캐릭터의 SkeletalMesh에 부착 (애니메이션 따라감)
	RushHitBox->SetCollisionProfileName(TEXT("NoCollision")); // 평소에는 꺼둠 (공격 시에만 킴)

	// 2. 모션 워핑 컴포넌트 생성
	// 스킬 사용 시 타겟 방향으로 회전하거나 이동 거리를 보정해주는 기능
	MotionWarpingComp = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComp"));

	// 3. 최적화: Tick 비활성화
	// 보스의 로직은 비헤이비어 트리와 GAS 이벤트로 돌아가므로 매 프레임 Tick을 돌릴 필요가 없음
	PrimaryActorTick.bCanEverTick = false;

	// [수정] 패턴 플래그 초기화 (안전장치)
	bPattern66Triggered = false;
	bPattern33Triggered = false;
}

// ---------------------------------------------------------------------------
// [3] BeginPlay (게임 시작 시 초기화)
// ---------------------------------------------------------------------------
void ABossDragon::BeginPlay()
{
	Super::BeginPlay();

	// [GAS] 체력 변화 감지 시스템 등록
	// AttributeSet의 Health 값이 변할 때마다 OnHealthChanged 함수가 자동으로 호출되도록 설정
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		// GetHealthAttribute()는 EnemyAttributeSet에 정의된 체력 변수
		ASC->GetGameplayAttributeValueChangeDelegate(UEnemyAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ABossDragon::OnHealthChanged);
	}
}

// ---------------------------------------------------------------------------
// [4] PostInitializeComponents (컴포넌트 초기화 후처리)
// ---------------------------------------------------------------------------
void ABossDragon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 히트박스 오버랩 이벤트 연결 (충돌 시 OnRushOverlapBegin 실행)
	if (RushHitBox)
	{
		RushHitBox->OnComponentBeginOverlap.AddDynamic(this, &ABossDragon::OnRushOverlapBegin);
	}
}

// ---------------------------------------------------------------------------
// [5] OnHealthChanged - 체력 기반 패턴 트리거 (66%, 33%)
// ---------------------------------------------------------------------------
void ABossDragon::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	float CurrentHealth = Data.NewValue; // 변경된 현재 체력
	float MaxHealth = 1.0f;

	// ASC에서 현재 MaxHealth 값을 가져옴 (버프/디버프 반영된 수치)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		MaxHealth = ASC->GetNumericAttribute(UEnemyAttributeSet::GetMaxHealthAttribute());
	}

	// 0 나누기 방지
	if (MaxHealth <= 0.0f) MaxHealth = 1.0f;

	// 체력 비율 계산 (0.0 ~ 1.0)
	float HealthRatio = CurrentHealth / MaxHealth;

	// --- 1차 전멸기 패턴 (체력 66% 이하 진입 시) ---
	if (HealthRatio <= 0.66f && !bPattern66Triggered)
	{
		bPattern66Triggered = true; // 중복 실행 방지 플래그 설정

		// 1. 전투 상태 초기화 (모든 행동 취소)
		ForceResetCombatState();

		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			if (UBlackboardComponent* Blackboard = AIC->GetBlackboardComponent())
			{
				// 1. 타깃 포인트의 위치(또는 액터 자체)를 블랙보드에 저장
				// "TargetLocation"은 블랙보드에 미리 만들어둘 Key 이름입니다.
				Blackboard->SetValueAsObject(TEXT("WipeLocation"), WipeTargetPoint);

				// 2. 패턴 시작 플래그 켜기
				Blackboard->SetValueAsBool(TEXT("IsCastingWipe"), true);
			}
		}

		// [수정] 직접 이벤트를 보내지 않고 AI(비헤이비어 트리)에게 위임합니다.
		UE_LOG(LogTemp, Warning, TEXT("[BossDragon] HP 66%% Reached! AI will handle the rest."));
	}

	// --- 2차 전멸기 패턴 (체력 33% 이하 진입 시) ---
	if (HealthRatio <= 0.33f && !bPattern33Triggered)
	{
		bPattern33Triggered = true;

		// 1. 전투 상태 초기화 (모든 행동 취소)
		ForceResetCombatState();
		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			if (UBlackboardComponent* Blackboard = AIC->GetBlackboardComponent())
			{
				// 1. 타깃 포인트의 위치(또는 액터 자체)를 블랙보드에 저장
				// "TargetLocation"은 블랙보드에 미리 만들어둘 Key 이름입니다.
				Blackboard->SetValueAsObject(TEXT("WipeLocation"), WipeTargetPoint);

				// 2. 패턴 시작 플래그 켜기
				Blackboard->SetValueAsBool(TEXT("IsCastingWipe"), true);
			}
		}
		// [수정] 직접 이벤트를 보내지 않고 AI(비헤이비어 트리)에게 위임합니다.
		UE_LOG(LogTemp, Warning, TEXT("[BossDragon] HP 33%% Reached! AI will handle the rest."));
	}
}

// ---------------------------------------------------------------------------
// [8-1] SpawnSafetyStairs - 전멸기 회피용 계단 생성
// ---------------------------------------------------------------------------
void ABossDragon::SpawnSafetyStairs(FVector CenterLocation, int32 MaxHeight, float LifeTime)
{
	if (!HasAuthority()) return;

	UWorld* World = GetWorld();
	// 새로운 인터페이스 접근 방식: static 함수인 Get()을 사용합니다.
	IBlockSystemInterface* BlockSystem = IBlockSystemInterface::Get(World);

	if (!BlockSystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[BossDragon] BlockSystemInterface Not Found!"));
		return;
	}

	// 인터페이스에서 제공하는 GridSize를 사용합니다. (보통 100.0f)
	const float GridSize = BlockSystem->GetGridSize();

	FVector SnappedCenter;
	SnappedCenter.X = FMath::RoundToFloat(CenterLocation.X / GridSize) * GridSize;
	SnappedCenter.Y = FMath::RoundToFloat(CenterLocation.Y / GridSize) * GridSize;
	SnappedCenter.Z = CenterLocation.Z;

	FVector Directions[] = { FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0), FVector(0,-1,0) };

	for (const FVector& Dir : Directions)
	{
		for (int32 Step = 1; Step <= MaxHeight; ++Step)
		{
			FVector SpawnPos = SnappedCenter + (Dir * (Step * GridSize));

			for (int32 H = 0; H < Step; ++H)
			{
				FVector HeightPos = SpawnPos;
				HeightPos.Z = SnappedCenter.Z + (H * GridSize);

				// 인터페이스의 IsLocationOccupied 사용
				if (!BlockSystem->IsLocationOccupied(HeightPos, GridSize))
				{
					// 인터페이스의 SpawnBlockByTag 사용
					AActor* NewBlock = BlockSystem->SpawnBlockByTag(
						TAG_Block_Type_Destructible,
						HeightPos,
						FRotator::ZeroRotator,
						false, // 중력 비활성화
						nullptr // 페이로드 생략
					);

					if (NewBlock)
					{
						NewBlock->SetLifeSpan(LifeTime);
						SpawnedStairsList.Add(NewBlock);
					}
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// [7] SetFloorWarningState - 바닥 장판 경고 (색상 변경 + 계단 삭제 기능 통합)
// ---------------------------------------------------------------------------
void ABossDragon::SetFloorWarningState(FVector CenterLocation, float Radius, bool bIsWarning)
{
	// [1] 경고가 꺼지는 상황(False)이라면 -> 생성했던 계단들도 즉시 삭제
	if (!bIsWarning)
	{
		DestroySpawnedStairs();
	}

	// [2] 태그 결정 (True면 공격 구역 표시, False면 해제)
	FGameplayTag TagToSend = bIsWarning ? TAG_Block_Highlight_AttackZone : TAG_Block_Highlight_AttackZone_None;

	// [3] 범위 내 블록 검색 설정
	TArray<FOverlapResult> Overlaps;
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this); // 보스 자신 제외

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Block); // 블록 전용 채널 사용

	bool bHit = GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		CenterLocation,
		FQuat::Identity,
		ObjectParams,
		SphereShape,
		Params
	);

	// [4] 검색된 블록들에게 색상 변경 이벤트 전송
	if (bHit)
	{
		FGameplayEventData EventData;
		EventData.Instigator = this;
		EventData.EventTag = TagToSend;

		for (const FOverlapResult& Result : Overlaps)
		{
			if (ABlockBase* Block = Cast<ABlockBase>(Result.GetActor()))
			{
				// BlockBase 내부의 네트워크 멀티캐스트 로직 실행
				Block->HandleGameplayEvent(TagToSend, EventData);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// [보조] DestroySpawnedStairs - 리스트에 있는 계단 강제 삭제
// ---------------------------------------------------------------------------
void ABossDragon::DestroySpawnedStairs()
{
	// 리스트에 저장된 모든 계단을 순회하며 파괴
	for (TWeakObjectPtr<AActor> StairActor : SpawnedStairsList)
	{
		if (StairActor.IsValid())
		{
			StairActor->Destroy(); // 즉시 삭제! (LifeTime 무시됨)
		}
	}

	SpawnedStairsList.Empty(); // 리스트 비우기
}


// ---------------------------------------------------------------------------
// [8] ExecuteHeightJudgmentKill - 높이 판정 즉사기 실행
// ---------------------------------------------------------------------------
void ABossDragon::ExecuteHeightJudgmentKill(float SafeHeightThreshold)
{
	// 월드의 모든 캐릭터 검색
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), FoundActors);

	float BossZ = GetActorLocation().Z; // 보스(바닥)의 높이

	for (AActor* Actor : FoundActors)
	{
		if (Actor == this) continue; // 보스 본인은 제외

		// "Player" 태그가 붙은 액터만 대상으로 함
		if (Actor->ActorHasTag(TEXT("Player")))
		{
			float PlayerZ = Actor->GetActorLocation().Z;
			float HeightDiff = PlayerZ - BossZ; // 플레이어가 보스보다 얼마나 위에 있는지

			// 플레이어가 안전 높이(계단 위)보다 낮다면 즉사 처리
			if (HeightDiff < SafeHeightThreshold)
			{
				// 대상의 GAS 컴포넌트 가져오기
				UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);

				// 미리 설정된 즉사 GameplayEffect(WipeDamageEffect) 적용
				if (TargetASC && WipeDamageEffect)
				{
					FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
					Context.AddSourceObject(this);

					// Spec(설계도) 생성 후 적용
					FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(WipeDamageEffect, 1.0f, Context);

					if (Spec.IsValid())
					{
						AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
						UE_LOG(LogTemp, Error, TEXT("!!! WIPE KILL !!! Player %s too low (Diff: %f)."), *Actor->GetName(), HeightDiff);
					}
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// [9] SetRushCollisionEnabled - 돌진 공격 히트박스 제어
// ---------------------------------------------------------------------------
void ABossDragon::SetRushCollisionEnabled(bool bEnable)
{
	if (!RushHitBox) return;

	if (bEnable)
	{
		// 공격 시작: 충돌 활성화 (Pawn만 감지)
		RushHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		RushHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		RushHitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
	else
	{
		// 공격 종료: 충돌 비활성화
		RushHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

// ---------------------------------------------------------------------------
// [10] OnRushOverlapBegin - 돌진 히트박스 충돌 시 데미지 적용
// ---------------------------------------------------------------------------
void ABossDragon::OnRushOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 자기 자신이나 유효하지 않은 액터는 무시
	if (OtherActor == this || !OtherActor) return;

	// 돌진 데미지(RushDamageEffect) 적용
	if (AbilitySystemComponent && RushDamageEffect)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
		if (TargetASC)
		{
			FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
			ContextHandle.AddSourceObject(this);

			FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(RushDamageEffect, 1.0f, ContextHandle);

			if (SpecHandle.IsValid())
			{
				AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// [11] UpdateMotionWarpTarget - 모션 워핑 타겟 갱신
// ---------------------------------------------------------------------------
void ABossDragon::UpdateMotionWarpTarget(AActor* TargetActor)
{
	// 보스가 스킬 애니메이션(몽타주) 재생 중일 때, 
	// 타겟의 위치를 실시간으로 추적하여 회전하거나 이동 거리를 맞춤
	if (MotionWarpingComp && TargetActor)
	{
		MotionWarpingComp->AddOrUpdateWarpTargetFromLocationAndRotation(
			FName("FaceTarget"), // 몽타주 내 NotifyState 이름과 일치해야 동작함
			TargetActor->GetActorLocation(),
			TargetActor->GetActorRotation()
		);
	}
}