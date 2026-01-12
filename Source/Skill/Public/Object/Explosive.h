// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"
#include "Explosive.generated.h"

class ABlockBase;

// 착륙 시점을 GA_Explosive에 알리기 위한 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExplosiveLanded);

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
	void Initialize(FVector StartLoc, ABlockBase* Target, float FlightDuration);

	// 폭발 로직 실행 (외부에서 호출)
	void Detonate();

	// 현재 부착되어 있는지 확인
	bool IsAttached() const { return bAttached; }

	// 착륙 알림 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Event")
	FOnExplosiveLanded OnLandedDelegate;

protected:
	// 목표 지점에 도착했을 때 처리
	void OnLanded();

	// 블록의 색상을 변경하는 헬퍼 함수
	void SetBlockColorRed(bool bEnable);

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
	ABlockBase* TargetBlock;

	float TotalFlightTime = 1.0f;
	float CurrentFlightTime = 0.0f;
	float ArcHeight = 300.0f; // 포물선 높이
};
