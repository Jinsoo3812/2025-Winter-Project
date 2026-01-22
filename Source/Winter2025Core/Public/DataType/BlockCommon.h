// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BlockCommon.generated.h"

class AChunkBase;

/**
 * 블록의 종류를 정의하는 Enum (기존 GameplayTag 대체)
 * 메모리 최적화를 위해 uint8 사용 (최대 255개 종류)
 */
UENUM(BlueprintType)
enum class EBlockType : uint8
{
	None 	UMETA(DisplayName = "None"), // 빈 공간
	Terrain		UMETA(DisplayName = "Terrain"),
	Destructible	UMETA(DisplayName = "Destructible")
	// 필요에 따라 추가
};

/**
 * 청크 내의 개별 블록들이 가지는 속성
 * 수많은 블록들이 BlockData가 런타임에 활용될 수 있으므로 가벼운 자료형만 사용
 */
USTRUCT(BlueprintType)
struct FBlockData
{
	GENERATED_BODY()

	// 블록 타입 (1 Byte)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBlockType Type = EBlockType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Health = 100.0f;

	UPROPERTY()
	bool bIsActorSpawned = false;

	// 회전 등 추가 가능(Bitmask) 
	/*
	* 메모리 패딩(4byte)을 고려하여 나머지 공간을 채울 것
	*/
};

// 6방향을 명확히 정의 (인덱스로 사용하기 위함)
enum class EBlockNeighbor : uint8
{
	Front = 0,  // X + 1
	Back,       // X - 1
	Right,      // Y + 1
	Left,       // Y - 1
	Up,         // Z + 1
	Down,       // Z - 1
	Count
};

/*
* BlockManangerSubsystem에 전달할 SpawnBlock 요청 데이터 구조체
*/
struct FBlockSpawnRequest
{	
	// 이 위치에
	FVector WorldLocation;
	// 이 블록을
	FGameplayTag BlockTag;
	// 이 청크 안에
	TWeakObjectPtr<AChunkBase> OwnerChunk;
};
