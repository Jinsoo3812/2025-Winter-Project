// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameplayEventInterface.h"
#include "BlockInfoInterface.h"
#include "BlockBase.generated.h"

class UDA_BlockConfig;


UCLASS()
class WORLD_API ABlockBase : public AActor, public IGameplayEventInterface, public IBlockInfoInterface
{
	GENERATED_BODY()
	
public:	
	ABlockBase();

protected:
	// -----------------------------------------------------------------------------
	// 초기화 함수들
	// -----------------------------------------------------------------------------

	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;

	// -----------------------------------------------------------------------------
	// BlockBase 고유
	// -----------------------------------------------------------------------------

	// 블록(정육면체)의 한 변의 길이
	UPROPERTY(EditDefaultsOnly, Category = "Grid")
	float GridSize = 100.0f;

	// 블록이 파괴 가능한지 여부를 담는 변수
	UPROPERTY(VisibleAnywhere, Category = "Block")
	bool IsDestrictible = false;

	// 블록의 외형 및 물리 충돌을 담당할 Mesh Component
	UPROPERTY(VisibleAnywhere, Category = "Block")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// 외형과 별개인 충돌 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = "Block")
	TObjectPtr<UBoxComponent> CollisionComponent;

	// 이 블록이 참조할 설정 파일 (블루프린트 디폴트에서 설정하거나, 스폰 시 주입)
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TObjectPtr<UDA_BlockConfig> BlockConfig;

	// 이 블록이 참조할 설정 파일 (블루프린트 디폴트에서 설정하거나, 스폰 시 주입)
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	int32 MaxBombCount;

	// 블록이 파괴 가능한지 여부를 반환하는 함수
	virtual bool CanBeDestroyed() const { return IsDestrictible; }

	// 자신을 파괴하는 함수
	virtual void SelfDestroy();

	// -----------------------------------------------------------------------------
	// 낙하 관련
	// -----------------------------------------------------------------------------

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

	// -----------------------------------------------------------------------------
	// 폭발 관련
	// -----------------------------------------------------------------------------

	void HandleBombEvent(const FGameplayTag& EventTag);

	// 현재 부착된 폭탄 개수 추적용
	int32 CurrentBombCount = 0;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// -----------------------------------------------------------------------------
	// BlockInfoInterface 구현
	// -----------------------------------------------------------------------------

	FVector GetBlockLocation() const override { return GetActorLocation(); }
	FRotator GetBlockRotation() const override { return GetActorRotation(); }
	float GetBlockGridSize() const override { return GridSize; }

	// -----------------------------------------------------------------------------
	// 헬퍼 함수
	// -----------------------------------------------------------------------------

	//  그리드에 정렬된 위치(GridSize 단위에 맞도록)를 반환하는 함수
	FVector GetBlockAlignedLocation() const override;

	// 블록의 메시 컴포넌트를 반환하는 함수 (머티리얼 변경 등에 사용)
	UStaticMeshComponent* GetBlockMesh() const { return MeshComponent; }

	// 낙하 가능 여부 설정 함수
	void SetCanFall(bool bNewCanFall) { bCanFall = bNewCanFall; }

	// -----------------------------------------------------------------------------
	// GameplayEventInterface 구현
	// -----------------------------------------------------------------------------

	/* GameplayEvent를 수신하는 함수 */
	void HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData& Payload) override;
};
