#include "Trigger/DragonTrigger.h"
#include "Components/BoxComponent.h"
#include "BossDragon.h"                 // BossDragon은 Public 폴더 바로 아래에 있으므로 그대로 씀
#include "AbilitySystemBlueprintLibrary.h" // GAS 이벤트 전송 라이브러리
#include "AbilitySystemComponent.h"



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

    // 1. 상대방(OtherActor)에게서 Ability System Component를 가져옵니다.
    UAbilitySystemComponent* OtherASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);

    // 2. ASC가 있고, 우리가 설정한 PlayerTag를 가지고 있는지 확인합니다.
    if (OtherASC && OtherASC->HasMatchingGameplayTag(PlayerTag))
    {
        if (TargetBoss && WakeUpTag.IsValid())
        {
            FGameplayEventData Payload;
            Payload.Instigator = OtherActor;
            Payload.EventTag = WakeUpTag;

            // 보스에게 이벤트 전송
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetBoss, WakeUpTag, Payload);

            if (bTriggerOnce)
            {
                SetActorEnableCollision(false);
            }
        }
    }
}
