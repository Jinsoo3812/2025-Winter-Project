// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Components/StaticMeshComponent.h"
#include "ABlockBase.generated.h"

UENUM()
enum class EBlockType : uint8 {
	IMMUTABLE UMETA(DisplayName = "Immutable"),
	Warning UMETA(DisplayName = "Warning"),
	Destructible UMETA(DisplayName = "Destructible"),
	Recordable UMETA(DisplayName = "Recordable")
};


UCLASS()
class WORLD_API AABlockBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AABlockBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Block")
	// 블록의 타입을 담는 변수
	EBlockType BlockType = EBlockType::IMMUTABLE;
	
	UPROPERTY(VisibleAnywhere, Category = "Block")
	// 블록의 위치를 담는 변수 (AActor에 BlockLocation이름이 이미 존재하여 Location으로 변경)
	FVector Location;

	// 블록이 파괴 가능한지 여부를 담는 변수
	UPROPERTY(VisibleAnywhere, Category = "Block")
	bool IsDestrictible = false;

	// 블록의 외형 및 물리 충돌을 담당할 Mesh Component
	UPROPERTY(VisibleAnywhere, Category = "Block")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// 블록의 위치와 타입 변수를 설정하고 소환합니다.
	UFUNCTION()
	virtual void SpawnBlock(FVector SpawnLocation, EBlockType NewBlockType);

	EBlockType GetBlockType() const { return BlockType; }
	FVector GetBlockLocation() const { return Location; }

	virtual bool CanBeDestroyed() const { return IsDestrictible; }
};
