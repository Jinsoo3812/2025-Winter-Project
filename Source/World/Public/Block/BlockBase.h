// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameplayEventInterface.h"
#include "BlockBase.generated.h"

class AChunkBase;
class UBlockConfig;


UCLASS()
class WORLD_API ABlockBase : public AActor, public IGameplayEventInterface
{
	GENERATED_BODY()

	// -----------------------------------------------------------------------------
	// 초기화 및 기본 함수
	// -----------------------------------------------------------------------------
public:	
	ABlockBase();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;

	// -----------------------------------------------------------------------------
	// Chunk
	// -----------------------------------------------------------------------------
public:
	void SetParentChunk(AChunkBase* InChunk);

protected:
	// 나를 생성한 청크에 대한 약한 참조
	UPROPERTY(Transient)
	TWeakObjectPtr<AChunkBase> ParentChunk;

	// -----------------------------------------------------------------------------
	// BlockBase 설정
	// -----------------------------------------------------------------------------
protected:
	// 블록(정육면체)의 한 변의 길이
	float GridSize = 100.0f;

	// 블록의 외형 및 물리 충돌을 담당할 Mesh Component
	UPROPERTY(VisibleAnywhere, Category = "Block")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// 외형과 별개인 충돌 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = "Block")
	TObjectPtr<UBoxComponent> CollisionComponent;

	// 각종 블록 매핑 설정값 (Setting에서 캐시)
	TObjectPtr<UBlockConfig> BlockConfig;

	// 최대 폭탄 부착 개수
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	int32 MaxBombCount;

public:
	// 블록의 메시 컴포넌트를 반환하는 함수 (머티리얼 변경 등에 사용)
	UStaticMeshComponent* GetBlockMesh() const { return MeshComponent; }

	// 자신을 파괴하는 함수
	virtual void SelfDestroy();

	// -----------------------------------------------------------------------------
	// 낙하 관련
	// -----------------------------------------------------------------------------
protected:
	// 낙하해도 되는 블록인지
	bool bCanFall = false;

	// 블록이 현재 낙하 중인지
	bool bIsFalling = false;

	// 현재 낙하 속도 (Z축)
	float VerticalVelocity = 0.0f;

	// 중력 가속도
	const float GravityAcceleration = -980.0f;

	// 낙하 중인지 반환하는 함수
	bool IsFalling() const { return bIsFalling; }

	// 낙하 로직을 처리하는 함수
	void UpdateGravity(float DeltaTime);

	// 바닥에 닿았는지 체크하고 그리드에 스냅하는 함수
	void CheckLanding();

	// 자신의 위 블록이 추락할 수 있도록 깨우는 함수
	void NotifyUpperBlock();
	
public:
	// 낙하 가능 여부 설정 함수
	void SetCanFall(bool bNewCanFall) { bCanFall = bNewCanFall; }

	// -----------------------------------------------------------------------------
	// 폭발 관련
	// -----------------------------------------------------------------------------
protected:
	// 현재 부착된 폭탄 개수 추적용
	int32 CurrentBombCount = 0;

	// -----------------------------------------------------------------------------
	// 하이라이트 관련
	// -----------------------------------------------------------------------------
protected:
	// CPD 업데이트 처리 함수
	void HandleHighlight(FGameplayTag EventTag, const FGameplayEventData& Payload);

	// -----------------------------------------------------------------------------
	// GameplayEventInterface 구현
	// -----------------------------------------------------------------------------
public:
	/* GameplayEvent를 수신하는 함수 */
	void HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData& Payload) override;
};
