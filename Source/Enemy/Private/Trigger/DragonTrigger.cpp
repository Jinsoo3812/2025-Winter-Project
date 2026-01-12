#include "Trigger/DragonTrigger.h"
#include "Components/BoxComponent.h"
#include "BossDragon.h"                 // BossDragon은 Public 폴더 바로 아래에 있으므로 그대로 씀
#include "AbilitySystemBlueprintLibrary.h" // GAS 이벤트 전송 라이브러리

ADragonTrigger::ADragonTrigger()
{
	// 1. 박스 컴포넌트 생성
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	// 2. 콜리전 프로필 설정 (Trigger 프리셋 사용)
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

	// 3. 겹침 이벤트 연결
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADragonTrigger::OnOverlapBegin);
}

void ADragonTrigger::BeginPlay()
{
	Super::BeginPlay();
}

void ADragonTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// [서버 권한 확인] 이 로직은 서버(Authority)에서만 실행
	if (!HasAuthority()) return;

	// [대상 확인] 들어온 녀석이 플레이어인가?
	if (OtherActor && OtherActor->ActorHasTag(TEXT("Player")))
	{
		// 보스가 연결되어 있고 태그가 올바른지 확인
		if (TargetBoss && WakeUpTag.IsValid())
		{
			// [GAS 이벤트 데이터 생성]
			FGameplayEventData Payload;
			Payload.Instigator = OtherActor; // 누가 깨웠는지
			Payload.EventTag = WakeUpTag;    // 무슨 신호인지

			// [이벤트 발송]
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetBoss, WakeUpTag, Payload);

			// 로그 (필요시 주석 해제)
			// UE_LOG(LogTemp, Log, TEXT("[DragonTrigger] Sent Event to %s"), *TargetBoss->GetName());

			// [일회용 처리]
			if (bTriggerOnce)
			{
				// 다시는 이벤트가 발생하지 않도록 콜리전을 꺼버림
				SetActorEnableCollision(false);
			}
		}
	}
}
