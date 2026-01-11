// BossDragon.h
#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h" // 부모 클래스
#include "BossDragon.generated.h"

class UBoxComponent; // 전방 선언

UCLASS()
class ENEMY_API ABossDragon : public AEnemyBase
{
	GENERATED_BODY()

public:
	ABossDragon();

	// [애님 노티파이에서 호출] 돌진 판정 박스 켜기/끄기
	void SetRushCollisionEnabled(bool bEnable);

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override; // 컴포넌트 세팅 완료 후 호출됨

	// [충돌 이벤트] 박스에 뭔가가 닿았을 때 실행
	UFUNCTION()
	void OnRushOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	// 1. 돌진 판정용 박스 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBoxComponent> RushHitBox;

	// 2. 돌진 데미지용 GameplayEffect (에디터에서 할당)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Combat")
	TSubclassOf<UGameplayEffect> RushDamageEffect;
};