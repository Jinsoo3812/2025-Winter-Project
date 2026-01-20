// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChunkBase.h"
#include "BlockMapManager.generated.h"

class UBlockConfig;

UCLASS()
class WORLD_API ABlockMapManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABlockMapManager();

protected:
	virtual void BeginPlay() override;

public:
	// -------------------------------------------------------------------------
	// 설정 (Blueprint에서 할당)
	// -------------------------------------------------------------------------

	// 블록 설정 데이터 에셋
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	UBlockConfig* BlockConfig;

	// 생성할 월드의 청크 개수 (예: 4x4)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	int32 WorldRangeX = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
	int32 WorldRangeY = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	int32 FloorHeight = 3;

	// -------------------------------------------------------------------------
	// 청크 관리
	// -------------------------------------------------------------------------

	// 청크 액터 맵 (ex. 첫 번째(0,0) 청크, 그 오른쪽의 두 번째(0,1) 청크 등)
	// Z는 보통 청크 하나로 높이를 커버하거나 필요하면 3D 키로 확장
	UPROPERTY(VisibleInstanceOnly, Category = "Map Manager")
	TMap<FIntPoint, AChunkBase*> ChunkMap;

	// 월드 생성 진입점
	void GenerateWorld();

	// 청크 간 이웃 설정
	void LinkChunkNeighbors();

	// 특정 월드 좌표(Vector)가 속한 청크 찾기
	AChunkBase* GetChunkAtLocation(FVector Location) const;

private:
	// 청크 스폰 및 초기화
	void SpawnChunks();

	// 테스트용: 간단한 지형 데이터 생성 (평지)
	void GenerateBasicTerrain();

	// 모든 청크에 비주얼 업데이트 요청
	void UpdateAllChunks();
};
