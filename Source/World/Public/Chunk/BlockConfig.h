// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BlockCommon.h"
#include "GameplayTagContainer.h"
#include "BlockConfig.generated.h"

/**
 * 단일 블록의 정의
 * 모든 블록이 공통으로 가지되, 그 타입별로 다르게 가지는 속성들
 */
USTRUCT(BlueprintType)
struct FBlockDefinition
{
	GENERATED_BODY()

	// 렌더링에 사용할 메시
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UStaticMesh* Mesh = nullptr;

	// 이 블록이 HISM이 아닌 Actor로 존재해야 하는지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Logic")
	bool bIsActor = false;

	// Actor로 스폰될 때 사용할 태그 (Subsystem 전달용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Logic", meta = (EditCondition = "bIsActor"))
	FGameplayTag ActorTag;

	// (추후 확장) 파괴 시 효과, 내구도 계수, 사운드 등
	// UPROPERTY(EditAnywhere, BlueprintReadOnly)
	// USoundBase* BreakSound;
};

UCLASS()
class WORLD_API UBlockConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	// Enum 키를 기반으로 블록 정의를 찾기 위한 맵
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Config")
	TMap<EBlockType, FBlockDefinition> BlockDefinitions;

	// 헬퍼 함수: 메시 가져오기
	UStaticMesh* GetMeshForType(EBlockType Type) const
	{
		if (const FBlockDefinition* Def = BlockDefinitions.Find(Type))
		{
			return Def->Mesh;
		}
		return nullptr;
	}
};
