// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChunkBase.h"
#include "BlockCommonTypes.h"
#include "BlockMapManager.generated.h"

class UBlockConfig;
class FNavigationLockContext;

/*
* 월드 내 청크들을 관리하는 매니저 액터
*/
UCLASS()
class WORLD_API ABlockMapManager : public AActor
{
	GENERATED_BODY()

	// -----------------------------------------------------------------------------
	// 초기화 및 기본 함수
	// -----------------------------------------------------------------------------
public:	
	// Sets default values for this actor's properties
	ABlockMapManager();

	// 혹시라도 락이 안 풀리고 파괴될 경우를 대비해 메모리 해제
	virtual ~ABlockMapManager();

protected:
	virtual void BeginPlay() override;

	// -------------------------------------------------------------------------
	// 기본 설정
	// -------------------------------------------------------------------------
public:
	// 생성할 월드의 청크 개수 X (예: 4x4)
	UPROPERTY(EditAnywhere, Category = "Config")
	int32 WorldRangeX = 4;

	// 생성할 월드의 청크 개수 Y (예: 4x4)
	UPROPERTY(EditAnywhere, Category = "Config")
	int32 WorldRangeY = 4;

	// 청크 하나의 X축 블록 개수
	UPROPERTY(EditAnywhere, Category = "Config")
	int32 ChunkSizeX = 16;

	// 청크 하나의 Y축 블록 개수
	UPROPERTY(EditAnywhere, Category = "Config")
	int32 ChunkSizeY = 16;

	// 청크 하나의 Z축 블록 개수
	UPROPERTY(EditAnywhere, Category = "Config")
	int32 ChunkSizeZ = 32;

	// 블록의 한 변의 길이
	int32 GridSize = 100;
protected:
	// 블록 설정 데이터 에셋
	UPROPERTY(EditAnywhere, Category = "Config")
	UBlockConfig* BlockConfig;

	// 테스트 용 기본 바닥의 층 수
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	int32 FloorHeight = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	EBlockType FloorBlockType = EBlockType::Terrain;

	// -------------------------------------------------------------------------
	// 청크 초기화
	// -------------------------------------------------------------------------
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TSubclassOf<AChunkBase> ChunkClass;

	// 월드 생성 진입점
	void GenerateWorld();

	// 청크 간 이웃 설정
	void LinkChunkNeighbors();

	// 청크 스폰 및 초기화
	void SpawnChunks();

	// 테스트용: 간단한 지형 데이터 생성 (평지)
	void GenerateBasicTerrain();

	// -------------------------------------------------------------------------
	// NavMesh 초기화
	// -------------------------------------------------------------------------
protected:
	// 초기 월드 생성 시 NavMesh 갱신을 정지하기 위한 타이머 핸들
	FTimerHandle InitialWorldGenTimerHandle;

	// 초기 월드 생성이 완료되었는지 확인하는 함수
	void CheckInitialWorldGenerationComplete();

	// 최초 맵 생성 시 NavMesh 갱신을 잠시 정지하기 위한 Lock 객체
	FNavigationLockContext* NavUpdateLock = nullptr;

	// -------------------------------------------------------------------------
	// 청크 관리
	// -------------------------------------------------------------------------
public:
	// 청크 액터 맵 (ex. 첫 번째(0,0) 청크, 그 오른쪽의 두 번째(0,1) 청크 등)
	UPROPERTY(VisibleInstanceOnly, Category = "Map Manager")
	TMap<FIntPoint, AChunkBase*> ChunkMap;

	// 특정 월드 좌표(Vector)가 속한 청크 찾기
	AChunkBase* GetChunkAtLocation(FVector Location) const;

	// 모든 청크에 비주얼 업데이트 요청
	void UpdateAllChunks();
};
