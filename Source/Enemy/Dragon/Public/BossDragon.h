// BossDragon.h
#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "MotionWarpingComponent.h"
#include "GameplayTagContainer.h" // GameplayTag 사용
#include "GameplayEffectTypes.h" // FOnAttributeChangeData 정의 헤더 포함 필요
#include "BossDragon.generated.h"

class UBoxComponent;
class UMotionWarpingComponent;
class UGameplayEffect;
class UNiagaraSystem;

UCLASS()
class ENEMY_API ABossDragon : public AEnemyBase
{
	GENERATED_BODY()

public:
	ABossDragon();

	// -------------------------------------------------------------------
	// [1] 체력 기반 패턴 트리거 (66%, 33%)
	// -------------------------------------------------------------------
	// 체력이 변할 때마다 호출되는 콜백 함수
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	// -------------------------------------------------------------------
	// [2] 즉사기 패턴 로직 (계단 소환, 경고, 처형)
	// -------------------------------------------------------------------

	// 계단 소환 (십자가 형태) - Subsystem을 사용하여 블록 생성 (지속시간 포함 버전)
	UFUNCTION(BlueprintCallable, Category = "Combat|Wipe")
	void SpawnSafetyStairs(FVector CenterLocation, int32 MaxHeight, float LifeTime = 15.0f);

	// 바닥 경고 표시 (빨간색) 및 [수정] 경고 해제 시 계단 삭제
	UFUNCTION(BlueprintCallable, Category = "Combat|Wipe")
	void SetFloorWarningState(FVector CenterLocation, float Radius, bool bIsWarning);

	// 높이 판정 처형 (즉사기 실행)
	UFUNCTION(BlueprintCallable, Category = "Combat|Wipe")
	void ExecuteHeightJudgmentKill(float SafeHeightThreshold = 200.0f);

	// [추가] 저장된 계단을 모두 삭제하는 함수 (내부적으로 호출됨)
	void DestroySpawnedStairs();

	// -------------------------------------------------------------------
	// [3] 기본 기능 (돌진, 모션워핑)
	// -------------------------------------------------------------------
	void SetRushCollisionEnabled(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void UpdateMotionWarpTarget(AActor* TargetActor);

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	// 충돌 이벤트
	UFUNCTION()
	void OnRushOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	// 컴포넌트들
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBoxComponent> RushHitBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MotionWarping")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComp;

	// GAS 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Combat")
	TSubclassOf<UGameplayEffect> RushDamageEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Combat")
	TSubclassOf<UGameplayEffect> WipeDamageEffect;

	// [설정] 패턴 발동용 태그 (Event.Boss.Pattern.Wipe)
	UPROPERTY(EditDefaultsOnly, Category = "GAS|Combat")
	FGameplayTag WipePatternEventTag;

	// 생성된 계단 블록들을 기억하는 배열 (TWeakObjectPtr로 안전하게 참조)
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SpawnedStairsList;

	// 에디터에서 직접 타깃 포인트를 지정할 변수
	// 패턴 진행 시 이동할 위치
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Combat|Wipe")
	TObjectPtr<AActor> WipeTargetPoint;


	// 에디터에서 할당할 평타 이펙트 에셋
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TObjectPtr<UNiagaraSystem> BasicAttackFX;

	// 이펙트를 소환할 함수
	UFUNCTION(BlueprintCallable, Category = "Effects")
	void PlayBasicAttackVFX(FName SocketName);



private:
	// 패턴 중복 실행 방지 플래그
	bool bPattern66Triggered = false;
	bool bPattern33Triggered = false;
};