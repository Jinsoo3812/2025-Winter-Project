// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "Bomb.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class IBlockSystemInterface;

/*
 * 스킬(GA)에서 폭탄(Actor)으로 전달할 Payload
 */
USTRUCT(BlueprintType)
struct FBombPayload
{
	GENERATED_BODY()

public:
	// 폭발 시 적용할 GE 클래스
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FGameplayEffectSpecHandle> EffectSpecs;

	// 폭발 반경
	UPROPERTY()
	float ExplosionRadius = 300.0f;

	// 자동 폭발 타이머 (0이면 자동 폭발 안 함)
	UPROPERTY()
	float AutoDetonateTime = 5.0f;

	// 폭탄을 던진 시전자의 ASC (GE 적용 시 Instigator로 사용)
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> InstigatorASC;

	// 부착 성공 시 시전자에게 보낼 이벤트 태그
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameplayTag AttachedEventTag;

	// 폭발 시 시전자에게 보낼 이벤트 태그
	UPROPERTY()
	FGameplayTag DetonationEventTag;

	// BlockSystem 캐시
	IBlockSystemInterface* BlockSystem;
};

UCLASS()
class SKILL_API ABomb : public AActor
{
	GENERATED_BODY()

	// -----------------------------------------------------------------------------
	// 초기화 및 기본 멤버
	// -----------------------------------------------------------------------------
public:
	ABomb();

	// 초기화 함수: 스폰 직후 GA가 호출
	void InitializeExplosive(const FBombPayload& InPayload);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// 전달받은 데이터 캐싱
	FBombPayload Payload;

	// -----------------------------------------------------------------------------
	// 충돌 및 부착
	// ------------------------------------------------------------------------------
public:
	// 물리 공식으로 발사 및 체공 (시간 기준)
	void LaunchByTime(FVector TargetLocation, float Time, float GravityZ);

protected:
	// 충돌 처리 (핵심 로직)
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// 부착 성공 시 호출
	void StickToTarget(USceneComponent* TargetComp, FName SocketName, const FHitResult& Hit);

	// 부착 가능 여부를 판단할 태그 (BlockGameplayTags에 정의되어 있다고 가정)
	// 예: Tag.State.StickyTarget
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FGameplayTag StickyTargetTag;

	// -----------------------------------------------------------------------------
	// 폭발
	// -----------------------------------------------------------------------------
public:
	// 폭발 함수
	UFUNCTION()
	void Detonate();

	// 이미 터졌는지 플래그 (중복 폭발 방지)
	bool bHasDetonated = false;

protected:
	// 자동 폭발 타이머 핸들
	FTimerHandle DetonateTimerHandle;
};

	
