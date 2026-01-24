// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BlockCommon.generated.h"

class AChunkBase;

/**
 * 釉붾줉??醫낅쪟瑜??뺤쓽?섎뒗 Enum (湲곗〈 GameplayTag ?泥?
 * 硫붾え由?理쒖쟻?붾? ?꾪빐 uint8 ?ъ슜 (理쒕? 255媛?醫낅쪟)
 */
UENUM(BlueprintType)
enum class EBlockType : uint8
{
	None 	UMETA(DisplayName = "None"), // 鍮?怨듦컙
	Terrain		UMETA(DisplayName = "Terrain"),
	Destructible	UMETA(DisplayName = "Destructible")
	// ?꾩슂???곕씪 異붽?
};

/**
 * 泥?겕 ?댁쓽 媛쒕퀎 釉붾줉?ㅼ씠 媛吏???곹깭
 * ?섎쭖? 釉붾줉?ㅼ씠 BlockData媛 ?고??꾩뿉 ?쒖슜?????덉쑝誘濡?媛踰쇱슫 ?먮즺?뺣쭔 ?ъ슜
 */
USTRUCT(BlueprintType)
struct FBlockData
{
	GENERATED_BODY()

	// 釉붾줉 ???(1 Byte)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBlockType Type = EBlockType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Health = 100.0f;

	UPROPERTY()
	bool bIsActorSpawned = false;

	// ?뚯쟾 ??異붽? 媛??Bitmask) 
	/*
	* 硫붾え由??⑤뵫(4byte)??怨좊젮?섏뿬 ?섎㉧吏 怨듦컙??梨꾩슱 寃?
	*/
};

// 6諛⑺뼢??紐낇솗???뺤쓽 (?몃뜳?ㅻ줈 ?ъ슜?섍린 ?꾪븿)
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
* BlockManangerSubsystem???꾨떖??SpawnBlock ?붿껌 ?곗씠??援ъ“泥?
*/
struct FBlockSpawnRequest
{	
	// ???꾩튂??
	FVector WorldLocation;
	// ??釉붾줉??
	FGameplayTag BlockTag;
	// ??泥?겕 ?덉뿉
	TWeakObjectPtr<AChunkBase> OwnerChunk;
};
