// Fill out your copyright notice in the Description page of Project Settings.


#include "BlockConfig.h"

const FBlockDefinition* UBlockConfig::GetBlockDef(const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UBlockConfig::GetBlockDef - Invalid GameplayTag provided."));
		return nullptr;
	}

	if (const int32* FoundIndex = TagToIndexMap.Find(Tag))
	{
		if (BlockDefinitions.IsValidIndex(*FoundIndex))
		{
			return &BlockDefinitions[*FoundIndex];
		}
	}

	return nullptr;
}

const FBlockDefinition* UBlockConfig::GetBlockDef(EBlockType Type) const
{
	if (Type == EBlockType::None)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBlockConfig::GetBlockDef - EBlockType::None provided."));
		return nullptr;
	}

	if (const int32* FoundIndex = TypeToIndexMap.Find(Type))
	{
		if (BlockDefinitions.IsValidIndex(*FoundIndex))
		{
			return &BlockDefinitions[*FoundIndex];
		}
	}

	return nullptr;
}

const FBlockDefinition* UBlockConfig::GetBlockDef(TSubclassOf<AActor> Class) const
{
	if (!Class)
	{
		UE_LOG(LogTemp, Warning, TEXT("UBlockConfig::GetBlockDef - Null Class provided."));
		return nullptr;
	}

	// TSubclassOf는 내부적으로 UClass*로 변환 가능하므로 키로 사용 가능
	if (const int32* FoundIndex = ClassToIndexMap.Find(Class))
	{
		if (BlockDefinitions.IsValidIndex(*FoundIndex))
		{
			return &BlockDefinitions[*FoundIndex];
		}
	}

	return nullptr;
}