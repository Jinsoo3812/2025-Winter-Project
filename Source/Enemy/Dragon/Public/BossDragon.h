// BossDragon.h
#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h" // 부모 클래스
#include "MotionWarpingComponent.h"
#include "Block/BlockBase.h"
#include "GameplayEffectTypes.h"
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
	

	// 모션 워핑 타겟 업데이트 함수 (블루프린트에서 호출 가능하게 설정)
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void UpdateMotionWarpTarget(AActor* TargetActor);

	// [패턴] 1. 안전지대(계단) 생성 함수
	UFUNCTION(BlueprintCallable, Category = "BossPattern")
	void SpawnSafetyStairs(FVector CenterLocation, int32 Radius, int32 MaxHeight);

	// [패턴] 2. 범위 내 모든 바닥 블록 빨간색(경고) 처리
	UFUNCTION(BlueprintCallable, Category = "BossPattern")
	void SetFloorWarningState(FVector CenterLocation, float Radius, bool bIsWarning);

	// [패턴] 3. 높이 판정 후 즉사 데미지 부여
	UFUNCTION(BlueprintCallable, Category = "BossPattern")
	void ExecuteHeightJudgmentKill(FVector CenterLocation, float Radius, float SafeHeightThreshold);

	// [패턴] 4. 생성된 안전지대(계단) 제거 함수
	UFUNCTION(BlueprintCallable, Category = "BossPattern")
	void DestroySafetyStairs(float Radius);

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override; // 컴포넌트 세팅 완료 후 호출됨

	// [충돌 이벤트] 박스에 뭔가가 닿았을 때 실행
	UFUNCTION()
	void OnRushOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	// 돌진 판정용 박스 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBoxComponent> RushHitBox;

	// 돌진 데미지용 GameplayEffect (에디터에서 할당)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Combat")
	TSubclassOf<UGameplayEffect> RushDamageEffect;


	// 모션 워핑 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MotionWarping")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComp;


	// 생성할 블록 클래스 (BP에서 지정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossPattern")
	TSubclassOf<ABlockBase> StairBlockClass;

	// 즉사 데미지 GE
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossPattern")
	TSubclassOf<UGameplayEffect> InstantDeathEffect;

	// 체력이 변할 때마다 호출될 콜백 함수
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	// 패턴이 이미 발동했는지 체크하는 플래그
	bool bPattern66Triggered = false;
	bool bPattern33Triggered = false;

	// 패턴 발동을 위한 이벤트 태그 (또는 어빌리티 클래스)
	// 에디터에서 "Event.Boss.Pattern.Wipe" 같은 태그를 지정할 예정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Pattern")
	FGameplayTag WipePatternEventTag;
};
