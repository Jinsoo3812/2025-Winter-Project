// BossDragon.cpp
#include "BossDragon.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h" // GAS 라이브러리 필수

ABossDragon::ABossDragon()
{
	// 1. 박스 컴포넌트 생성
	RushHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("RushHitBox"));
	// 메쉬에 붙입니다 (나중에 에디터에서 소켓이나 위치 조정 가능)
	RushHitBox->SetupAttachment(GetMesh());
	// 평소에는 꺼둡니다 (NoCollision)
	RushHitBox->SetCollisionProfileName(TEXT("NoCollision"));
}

void ABossDragon::BeginPlay()
{
	Super::BeginPlay();
	// 부모(EnemyBase)의 로직(HP바 생성, GAS 초기화 등)이 실행됩니다.
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