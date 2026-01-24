// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BlockCommonTypes.h"
#include "BlockConfig.generated.h"

UCLASS()
class WORLD_API UBlockConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	// Enum 키를 기반으로 블록 정의를 찾기 위한 맵
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Config")
	TMap<EBlockType, FBlockDefinition> BlockDefinitions;

	// GameplayTag를 기반으로 CPD 값을 연결하기 위한 맵
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Highlight Config")
	TMap<FGameplayTag, FBlockCPDInfo> HighlightSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Config")
	int32 BombCPDIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block Config")
	float BombIntensityPerCount = 0.33f;

	// EBlockType으로 메시를 가져오는 헬퍼 함수
	UStaticMesh* GetMeshForType(EBlockType Type) const
	{
		if (const FBlockDefinition* Def = BlockDefinitions.Find(Type))
		{
			return Def->Mesh;
		}
		return nullptr;
	}

	// GameplayTag로 EBlockType을 찾는 헬퍼 함수
	EBlockType GetBlockTypeByTag(const FGameplayTag& Tag) const
	{
		// 맵을 순회하며 태그가 일치하는지 확인
		for (const auto& Pair : BlockDefinitions)
		{
			if (Pair.Value.bIsActor && Pair.Value.ActorTag.MatchesTag(Tag))
			{
				return Pair.Key;
			}
		}
		// 못 찾으면 None 반환
		return EBlockType::None;
	}

	// ------------------------------------------------------------
	// 이주용 임시 함수들
	// ------------------------------------------------------------

	// Tag로 ActorClass 찾기 (DA_BlockConfig 대체용)
	TSubclassOf<AActor> GetBlockClassByTag(const FGameplayTag& Tag) const
	{
		for (const auto& Pair : BlockDefinitions)
		{
			if (Pair.Value.bIsActor && Pair.Value.ActorTag.MatchesTag(Tag))
			{
				return Pair.Value.ActorClass;
			}
		}
		return nullptr;
	}

	// Tag로 CPDInfo 찾기 (DA_BlockConfig 대체용)
	FBlockCPDInfo GetHighlightInfoByTag(const FGameplayTag& Tag) const
	{
		if (const FBlockCPDInfo* Info = HighlightSettings.Find(Tag))
		{
			return *Info;
		}

		// 못 찾으면 기본값(0,0) 반환
		return FBlockCPDInfo();
	}
};
