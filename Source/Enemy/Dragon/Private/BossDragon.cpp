// BossDragon.cpp
#include "BossDragon.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h" // GAS 라이브러리 필수
#include "MotionWarpingComponent.h"
#include "Kismet/KismetSystemLibrary.h" // UKismetSystemLibrary 사용을 위해 필수
#include "Engine/OverlapResult.h"       // FOverlapResult 사용을 위해 필수
#include "Engine/EngineTypes.h"         // EObjectTypeQuery 등 엔진 타입 사용
#include "EnemyAttributeSet.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

ABossDragon::ABossDragon()
{
	// 1. 박스 컴포넌트 생성
	RushHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RushHitBox"));
	// 메쉬에 붙입니다 (나중에 에디터에서 소켓이나 위치 조정 가능)
	RushHitBox->SetupAttachment(GetMesh());
	// 평소에는 꺼둡니다 (NoCollision)
	RushHitBox->SetCollisionProfileName(TEXT("NoCollision"));

	// 캐릭터가 모션 워핑 기능을 가질 수 있게 해줍니다.
	MotionWarpingComp = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComp"));
}

void ABossDragon::BeginPlay()
{
    // [중요] 부모(EnemyBase)의 BeginPlay를 호출해야 
    // 위에서 작성한 GAS 초기화, 스탯 적용, 스킬 부여가 자동으로 실행됩니다.
    Super::BeginPlay();

    // 4. 체력 변경 감지 델리게이트 등록 (패턴 발동용)
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // 체력(Health) 값이 변할 때마다 OnHealthChanged 함수를 실행하도록 연결
        // UEnemyAttributeSet::GetHealthAttribute()는 매크로가 생성한 함수입니다.
        ASC->GetGameplayAttributeValueChangeDelegate(UEnemyAttributeSet::GetHealthAttribute())
            .AddUObject(this, &ABossDragon::OnHealthChanged);

        UE_LOG(LogTemp, Log, TEXT("[BossDragon] Ready to check HP thresholds."));
    }
}

void ABossDragon::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 2. 충돌 이벤트 연결 (BeginPlay보다 안전한 위치)
	if (RushHitBox)
	{
		RushHitBox->OnComponentBeginOverlap.AddDynamic(this, &ABossDragon::OnRushOverlapBegin);
	}
}

void ABossDragon::SetRushCollisionEnabled(bool bEnable)
{
	if (!RushHitBox) return;

	if (bEnable)
	{
		// 켜기: 적(Pawn)만 감지하도록 설정 (QueryOnly)
		RushHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		// 모든 채널 무시 후 Pawn(플레이어)만 겹침(Overlap) 허용
		RushHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		RushHitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
	else
	{
		// 끄기: 다시 무적 상태
		RushHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABossDragon::OnRushOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 자기 자신이나 null은 무시
	if (OtherActor == this || !OtherActor) return;

	// [데미지 적용 로직]
	// 부모(EnemyBase)가 가진 AbilitySystemComponent를 사용
	// (EnemyBase의 ASC 접근 권한이 protected 이상이어야 함. 만약 private라면 GetAbilitySystemComponent() 사용)
	if (AbilitySystemComponent && RushDamageEffect)
	{
		// 타겟(플레이어)의 ASC 가져오기
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
		if (TargetASC)
		{
			// 데미지 스펙 생성 (Context + Spec)
			FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
			ContextHandle.AddSourceObject(this);

			// 레벨 1.0 기준으로 Effect 생성
			FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(RushDamageEffect, 1.0f, ContextHandle);

			if (SpecHandle.IsValid())
			{
				// 데미지 적용!
				AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

				// 로그 출력
				UE_LOG(LogTemp, Warning, TEXT("[BossDragon] Rush HIT! Damaged Actor: %s"), *OtherActor->GetName());
			}
		}
	}
}


// 이 함수는 블루프린트(비헤이비어 트리 서비스 등)에서 호출할 예정입니다.
void ABossDragon::UpdateMotionWarpTarget(AActor* TargetActor)
{
	if (MotionWarpingComp && TargetActor)
	{
		// "FaceTarget": 몽타주 노티파이에서 사용할 이름입니다. (꼭 기억하세요!)
		// 타겟의 현재 위치와 회전값을 'FaceTarget'이라는 지점으로 등록합니다.
		MotionWarpingComp->AddOrUpdateWarpTargetFromLocationAndRotation(
			FName("FaceTarget"),
			TargetActor->GetActorLocation(),
			TargetActor->GetActorRotation()
		);

		// 로그로 확인 (디버깅용)
		// UE_LOG(LogTemp, Log, TEXT("[BossDragon] Motion Warp Target Updated: %s"), *TargetActor->GetName());
	}
}

void ABossDragon::SpawnSafetyStairs(FVector CenterLocation, int32 Radius, int32 MaxHeight)
{
    // 서버에서만 실행
    if (!HasAuthority() || !StairBlockClass) return;

    UWorld* World = GetWorld();
    if (!World) return;

    float GridSize = 100.0f; // BlockBase의 그리드 사이즈에 맞춤

    // 중심 좌표를 그리드에 맞게 스냅 (정확한 블록 위치 계산 위함)
    FVector SnappedCenter;
    SnappedCenter.X = FMath::RoundToFloat(CenterLocation.X / GridSize) * GridSize;
    SnappedCenter.Y = FMath::RoundToFloat(CenterLocation.Y / GridSize) * GridSize;
    SnappedCenter.Z = CenterLocation.Z; // 바닥 높이는 유지한다고 가정

    // 예시: 보스 주변 4방향에 계단 생성 (십자가 형태)
    // 1칸 거리: 1층, 2칸 거리: 2층, 3칸 거리: 3층 (안전)

    // 방향 벡터 배열 (앞, 뒤, 좌, 우)
    FVector Directions[] = { FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0), FVector(0,-1,0) };

    for (const FVector& Dir : Directions)
    {
        for (int32 Step = 1; Step <= MaxHeight; ++Step)
        {
            // 거리만큼 떨어진 위치 계산
            FVector SpawnPos = SnappedCenter + (Dir * (Step * GridSize));

            // 높이 쌓기 (1층부터 Step층까지)
            // 예: Step이 3이면, 1층, 2층, 3층에 모두 블록을 소환해야 계단이 됨
            for (int32 H = 0; H < Step; ++H)
            {
                // Z축 오프셋 (블록 높이만큼)
                // 기본 바닥이 0이라면 0, 100, 200 위치에 생성
                FVector HeightPos = SpawnPos;
                HeightPos.Z = SnappedCenter.Z + (H * GridSize);

                // BlockBase에 있는 정적 함수 혹은 로직 활용
                // *중요*: 이미 블록이 있는지 체크하고 없으면 소환
                if (!ABlockBase::IsLocationOccupied(World, HeightPos, GridSize))
                {
                    // ABlockBase에 있는 Spawn helper 사용 권장 (헤더에 static으로 변환하거나 직접 구현)
                    FActorSpawnParameters SpawnParams;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                    ABlockBase* NewBlock = World->SpawnActor<ABlockBase>(StairBlockClass, HeightPos, FRotator::ZeroRotator, SpawnParams);
                    if (NewBlock)
                    {
                        NewBlock->SpawnBlock(HeightPos, EBlockType::Destructible); // BlockBase의 초기화 함수 호출
                        NewBlock->SetActorTickEnabled(false); // 계단은 떨어지면 안되므로 물리 끔
                    }
                }
            }
        }
    }
}

void ABossDragon::SetFloorWarningState(FVector CenterLocation, float Radius, bool bIsWarning)
{
    // 1. 범위 내 블록 찾기 (GA_Construction의 FindBlocksInRange와 유사한 역할)
    TArray<FOverlapResult> Overlaps;
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); // 보스 자신은 제외

    // WorldStatic(블록)만 검사
    bool bHit = GetWorld()->OverlapMultiByObjectType(
        Overlaps,
        CenterLocation,
        FQuat::Identity,
        FCollisionObjectQueryParams(ECC_WorldStatic), // 블록 채널
        SphereShape,
        Params
    );

    if (bHit)
    {
        for (const FOverlapResult& Result : Overlaps)
        {
            // 2. 블록인지 확인 (GA_Construction의 Cast 부분)
            if (ABlockBase* Block = Cast<ABlockBase>(Result.GetActor()))
            {
                // 3. 색상 변경 (GA_Construction의 HighlightBlocksInRange 로직 응용)
                // UStaticMeshComponent 가져오기
                if (UStaticMeshComponent* BlockMesh = Block->GetBlockMesh())
                {
                    if (bIsWarning)
                    {
                        // 여기도 BlockMesh로 변경
                        BlockMesh->SetCustomPrimitiveDataFloat(0, 3.0f);
                    }
                    else
                    {
                        // 여기도 BlockMesh로 변경
                        BlockMesh->SetCustomPrimitiveDataFloat(0, 0.0f);
                    }
                }
            }
        }
    }
}

//이코드가 호출되면 지정된 반경 내의 플레이어 캐릭터들을 검사하여 높이가 안전 기준치 이하인 경우 즉사 처리합니다.
void ABossDragon::ExecuteHeightJudgmentKill(FVector Center, float Radius, float SafeHeightThreshold)
{
    // 1. 범위 감지 (Pawn, WorldDynamic 등)
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

    TArray<AActor*> OutActors;
    UKismetSystemLibrary::SphereOverlapActors(GetWorld(), Center, Radius, ObjectTypes, nullptr, TArray<AActor*>(), OutActors);

    // 2. 필터링
    for (AActor* Target : OutActors)
    {
        // 1. 캐릭터 확인 (바닥 제외)
        ACharacter* PlayerChar = Cast<ACharacter>(Target);
        if (!PlayerChar) continue;

        // 2. 높이 확인 (커트라인보다 낮으면 사망)
        float TargetZ = PlayerChar->GetActorLocation().Z;
        if (TargetZ < SafeHeightThreshold)
        {
            // [처형 로직 시작]
            if (InstantDeathEffectClass)
            {
                // 타겟(플레이어)의 ASC 가져오기
                UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerChar);

                if (TargetASC)
                {
                    // GE 적용을 위한 Context 생성 (누가 때렸나?)
                    FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
                    ContextHandle.AddSourceObject(this); // 보스가 원인 제공자

                    // GE Spec(설계도) 생성
                    FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(InstantDeathEffectClass, 1.0f, ContextHandle);

                    if (SpecHandle.IsValid())
                    {
                        // 쾅! 즉사 효과 적용
                        TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

                        UE_LOG(LogTemp, Error, TEXT("☠️ EXECUTION COMPLETED: %s"), *PlayerChar->GetName());
                    }
                }
            }
            else
            {
                // GE가 할당 안 됐을 때를 대비한 비상용 일반 데미지 (선택 사항)
                // UGameplayStatics::ApplyDamage(PlayerChar, 99999.f, GetController(), this, UDamageType::StaticClass());
                UE_LOG(LogTemp, Warning, TEXT("InstantDeathEffectClass is NULL! Assign it in BP!"));
            }
        }
    }
}

// 체력이 변할 때마다 자동으로 호출됨 (데미지를 입거나 회복할 때)
void ABossDragon::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    // 1. 현재 체력 (변경된 값)
    // Data.NewValue에 변경된 현재 체력이 들어옵니다.
    float CurrentHealth = Data.NewValue;
    
    
    //////////
    float OldHealth = Data.OldValue; // 이전 체력 (얼마나 깎였는지 볼 때 유용)
    float DamageTaken = OldHealth - CurrentHealth;
    //////////
    
    
    // 2. 최대 체력 가져오기
    float MaxHealth = 1.0f; // 0 나누기 방지용 기본값

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // ASC에게 "MaxHealth 속성의 현재 수치값(Numeric Value)을 줘"라고 요청하는 코드입니다.
        // UEnemyAttributeSet::GetMaxHealthAttribute()는 매크로가 만들어준 '속성 식별자'를 반환합니다.
        // 이 코드는 제공해주신 헤더 파일과 100% 호환됩니다.
        MaxHealth = ASC->GetNumericAttribute(UEnemyAttributeSet::GetMaxHealthAttribute());
    }

    /////////////
    if (GEngine)
    {
        FString DebugMsg = FString::Printf(TEXT("Boss HP: %.0f / %.0f (Damage: %.0f)"), CurrentHealth, MaxHealth, DamageTaken);
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, DebugMsg);
    }

    // [추가] 출력 로그에도 남기기
    UE_LOG(LogTemp, Warning, TEXT("[BossDragon] Hit! HP: %.1f -> %.1f (Max: %.1f)"), OldHealth, CurrentHealth, MaxHealth);
    ////////////


    // 방어 코드: MaxHealth가 0이거나 실패했을 경우 1로 설정
    if (MaxHealth <= 0.0f) MaxHealth = 1.0f;

    // 3. 체력 비율 계산
    float HealthRatio = CurrentHealth / MaxHealth;


    // --- [패턴 발동 로직] ---

    // 66% 패턴 체크 (한 번만 실행되도록 플래그 체크)
    if (HealthRatio <= 0.66f && !bPattern66Triggered)
    {
        bPattern66Triggered = true;

        FGameplayEventData Payload;
        Payload.EventTag = WipePatternEventTag; // 에디터에서 설정한 태그 (Event.Boss.Pattern.Wipe)
        Payload.Instigator = this;
        Payload.Target = this;

        // 즉사기 패턴 발동 신호 보냄
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, WipePatternEventTag, Payload);

        UE_LOG(LogTemp, Warning, TEXT("[BossDragon] HP 66%% Reached! Current: %.1f / Max: %.1f"), CurrentHealth, MaxHealth);
    }

    // 33% 패턴 체크
    if (HealthRatio <= 0.33f && !bPattern33Triggered)
    {
        bPattern33Triggered = true;

        FGameplayEventData Payload;
        Payload.EventTag = WipePatternEventTag;
        Payload.Instigator = this;
        Payload.Target = this;

        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, WipePatternEventTag, Payload);

        UE_LOG(LogTemp, Warning, TEXT("[BossDragon] HP 33%% Reached! Current: %.1f / Max: %.1f"), CurrentHealth, MaxHealth);
    }
}

// 안전 계단 제거 함수 (패턴 종료 후 호출)
void ABossDragon::DestroySafetyStairs(float Radius)
{
    if (!HasAuthority()) return;

    TArray<AActor*> FoundActors;
    // 내 주변의 "계단 블록(StairBlockClass)"만 찾아서 파괴
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), StairBlockClass, FoundActors);

    for (AActor* Actor : FoundActors)
    {
        // 거리 체크 (너무 멀리 있는 건 안 건드리기 위함, 필요 없으면 생략 가능)
        if (GetDistanceTo(Actor) <= (Radius * 100.0f + 500.0f))
        {
            Actor->Destroy();
        }
    }
}