// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DestructibleBlock.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "BarrierBlock.generated.h"

/**
 * GA에 의해 생성되고, 전진 및 폭파하는 장벽 블록 클래스
 */
UCLASS()
class WORLD_API ABarrierBlock : public ADestructibleBlock
{
	GENERATED_BODY()
	
	// -------------------------------------------------------------
	// 초기화 및 기본 함수
	// ------------------------------------------------------------- 
public:
	ABarrierBlock();

	// UBlockSpawnPayload를 이용해 멤버 변수 초기화
	virtual void InitializeBlock(const UBlockSpawnPayload* InPayload) override;

protected:
	virtual void BeginPlay() override;

	// -------------------------------------------------------------
	// Gameplay Tags
	// ------------------------------------------------------------- 
protected:
	// 아군(플레이어)은 밀어내기 위한 태그
	FGameplayTag TeamAllyTag;

	// 적(다른 블록, 적)과의 충돌에서 폭발하기 위한 태그
	FGameplayTag TeamEnemyTag;

	// -------------------------------------------------------------------------
	// 발사 설정
	// -------------------------------------------------------------------------
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	UProjectileMovementComponent* ProjectileMovement;

	// 발사 속도 
	UPROPERTY()
	float LaunchSpeed = 1500.0f;

	// 발사 후 최대 수명
	UPROPERTY()
	float MaxLifeTime = 3.0f;

	// 수명 타이머 핸들
	FTimerHandle LifeTimerHandle;

	// 아군 넉백 힘
	UPROPERTY()
	float AllyKnockbackStrength = 800.0f;

	// Payload를 통해 전달받은 GE Spec
	FGameplayEffectSpecHandle DamageSpecHandle;

	// -------------------------------------------------------------------------
	// 스킬 동작 
	// -------------------------------------------------------------------------
public:
	/* 블록을 발사하는 함수
	* Skill 모듈에서 간단하게 사용하기 위해 UFUNCTION으로 노출
	* @param Direction 발사할 방향
	*/
	UFUNCTION(BlueprintCallable)
	void Launch(FVector Direction);

protected:
	// 충돌 이벤트 처리
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp,
		AActor* OtherActor,UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	// 폭발 처리 (적중 또는 지형 충돌 시)
	void Explode();
};
