// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "BlockBase.generated.h"

UENUM()
enum class EBlockType : uint8 {
	IMMUTABLE UMETA(DisplayName = "Immutable"),
	Warning UMETA(DisplayName = "Warning"),
	Destructible UMETA(DisplayName = "Destructible"),
	Recordable UMETA(DisplayName = "Recordable")
};


UCLASS()
class WORLD_API ABlockBase : public AActor
{
	GENERATED_BODY()
	
public:	
	ABlockBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Block")
	// 블록의 타입을 담는 변수
	EBlockType BlockType = EBlockType::IMMUTABLE;

	UPROPERTY(EditDefaultsOnly, Category = "Grid")
	float GridSize = 100.0f;
	
	UPROPERTY(VisibleAnywhere, Category = "Block")
	// 블록의 위치를 담는 변수 (AActor에 BlockLocation이름이 이미 존재하여 Location으로 변경)
	FVector Location;

	// 블록이 파괴 가능한지 여부를 담는 변수
	UPROPERTY(VisibleAnywhere, Category = "Block")
	bool IsDestrictible = false;

	// 블록의 외형 및 물리 충돌을 담당할 Mesh Component
	UPROPERTY(VisibleAnywhere, Category = "Block")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// 외형과 별개인 충돌 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = "Block")
	TObjectPtr<UBoxComponent> CollisionComponent;

	// BP에서 설정 가능한 기본 메시 (기본값: Cube)
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	TObjectPtr<UStaticMesh> DefaultBlockMesh;

	// 낙하해도 되는 블록인지
	bool bCanFall = false;

	// 블록이 현재 낙하 중인지
	bool bIsFalling = false;

	// 현재 낙하 속도 (Z축)
	float VerticalVelocity = 0.0f;

	// 중력 가속도
	const float GravityAcceleration = -980.0f;

	// 낙하 로직을 처리하는 함수
	void UpdateGravity(float DeltaTime);

	// 바닥에 닿았는지 체크하고 그리드에 스냅하는 함수
	void CheckLanding();

	// 자신의 위 블록이 추락할 수 있도록 깨우는 함수
	void NotifyUpperBlock();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// 블록의 위치와 타입 변수를 설정하고 소환합니다.
	UFUNCTION()
	virtual void SpawnBlock(FVector SpawnLocation, EBlockType NewBlockType);

	EBlockType GetBlockType() const { return BlockType; }
	FVector GetBlockLocation() const { return Location; }
	float GetGridSize() const { return GridSize; }

	virtual bool CanBeDestroyed() const { return IsDestrictible; }

	// 블록의 메시 컴포넌트를 반환하는 함수 (머티리얼 변경 등에 사용)
	UStaticMeshComponent* GetBlockMesh() const { return MeshComponent; }

	bool IsFalling() const { return bIsFalling; }

	void SetCanFall(bool bNewCanFall) { bCanFall = bNewCanFall; }

};
