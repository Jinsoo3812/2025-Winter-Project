#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h" // GAS 태그(FGameplayTag) 사용을 위해 필수
#include "DragonTrigger.generated.h"

// 전방 선언: 헤더에서는 "이런 클래스가 있다"고만 알려주고, 실제 include는 cpp에서 합니다.
class UBoxComponent;
class ABossDragon;

UCLASS()
class ENEMY_API ADragonTrigger : public AActor
{
	GENERATED_BODY()

public:
	ADragonTrigger();

protected:
	virtual void BeginPlay() override;

public:
	// -------------------------------------------------------------------------
	// 컴포넌트
	// -------------------------------------------------------------------------

	// 트리거 영역 (박스)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger")
	UBoxComponent* TriggerBox;

	// -------------------------------------------------------------------------
	// 설정 변수
	// -------------------------------------------------------------------------

	// 깨울 보스 (에디터에서 스포이드로 지정)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Trigger")
	ABossDragon* TargetBoss;

	// 보스에게 보낼 이벤트 태그 (예: Event.Boss.WakeUp)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FGameplayTag WakeUpTag;

	// 한 번만 작동할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	bool bTriggerOnce = true;

private:
	// 겹침 감지 함수
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};