// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"
#include "GameplayEffectTypes.h"
#include "Explosive.generated.h"

class ABlockBase;
class ABlockBase;
class UAbilitySystemComponent;
class UGameplayEffect;

// 폭발 시점을 GA_Explosive에 알리기 위한 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExplosiveDetonated);

UCLASS()
class SKILL_API AExplosive : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AExplosive();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// 초기화 함수: 시작점, 목표 블록, 비행 시간을 설정
	// @param StartLoc: 폭발물의 시작 위치
	// @param Target: 목표 블록
	// @param FlightDuration: 목표까지 도달하는 데 걸리는 시간 (초)
	// @param InAutoDetonateDelay: 자동 기폭 대기 시간 (초)
	// @param InExplosionRadius: 폭발 반경
	// @param InMaxBombCount: 최대 투척 폭탄 개수
	// @
	// @param InSourceASC: 데미지 적용에 사용할 소스 ASC
	// @param InDamageSpecHandle: 데미지 적용에 사용할 GE Spec Handle
	// @param InDestructionEffectClass: 파괴 효과로 적용할 GE 클래스
	void Initialize(
		FVector StartLoc,
		AActor* Target,
		float FlightDuration,
		float InAutoDetonateDelay,
		float InExplosionRadius,
		int32 InMaxBombCount,
		UAbilitySystemComponent* InSourceASC,
		FGameplayEffectSpecHandle InDamageSpecHandle,
		TSubclassOf<UGameplayEffect> InDestructionEffectClass)
	);

	// 폭발 로직 실행 (외부에서 호출)
	void Detonate();

	// 현재 부착되어 있는지 확인
	bool IsAttached() const { return bAttached; }

	// 폭발 알림 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Event")
	FOnExplosiveDetonated OnDetonatedDelegate;

protected:
	// 목표 지점에 도착했을 때 처리
	void OnLanded();

	// 자동 폭파 타이머에 의해 호출됨
	UFUNCTION()
	void OnAutoDetonate();

	// 블록이 파괴되었을 때 동시에 파괴되기 위한 콜백 함수
	UFUNCTION()
	void OnBlockDestroyed(AActor* DestroyedActor);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bAttached = false;

	/**
	* 비행 관련 변수
	*/

	// 시작 위치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flight")
	FVector StartLocation;

	// 목표 블록의 위치(목표 블록의 윗면 중앙)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flight")
	FVector TargetLocation;

	// 목표 블록 포인터
	UPROPERTY()
	TWeakObjectPtr<AActor> TargetBlock;

	float TotalFlightTime = 1.0f;
	float CurrentFlightTime = 0.0f;
	float ArcHeight = 300.0f; // 포물선 높이

	/**
	 *  폭발 관련 변수
	 */
	float AutoDetonateDelay = 3.0f;
	float ExplosionRadius = 300.0f;

	// 데미지 적용을 위한 시전자의 ASC
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;

	// 데미지 스펙 핸들 (스냅샷 된 데미지 데이터)
	FGameplayEffectSpecHandle DamageSpecHandle;

	// 추가 파괴 이펙트 클래스
	UPROPERTY()
	TSubclassOf<UGameplayEffect> DestructionEffectClass;

	// 자동 폭파 타이머 핸들
	FTimerHandle DetonateTimerHandle;

	int32 MaxBombCount = 3;
};
