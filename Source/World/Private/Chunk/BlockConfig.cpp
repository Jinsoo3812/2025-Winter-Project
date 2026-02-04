// Fill out your copyright notice in the Description page of Project Settings.


#include "BlockConfig.h"

UStaticMesh* UBlockConfig::GetMeshForType(EBlockType Type) const
{
	if (const FBlockDefinition* Def = BlockDefinitions.Find(Type))
	{
		return Def->Mesh;
	}
	return nullptr;
}

EBlockType UBlockConfig::GetBlockTypeByTag(const FGameplayTag& Tag) const
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

FGameplayTag UBlockConfig::GetBlockTagByClass(TSubclassOf<AActor> InClass) const
{
	if (!InClass)
	{
		return FGameplayTag::EmptyTag;
	}

	// 맵을 순회하며 Class가 일치하는지 확인
	for (const auto& Pair : BlockDefinitions)
	{
		// 설정된 클래스와 현재 인스턴스의 클래스가 일치하는지 확인
		if (Pair.Value.bIsActor && Pair.Value.ActorClass == InClass)
		{
			return Pair.Value.ActorTag;
		}
	}

	// 못 찾으면 EmptyTag 반환
	return FGameplayTag::EmptyTag;
}

TSubclassOf<AActor> UBlockConfig::GetBlockClassByTag(const FGameplayTag & Tag) const
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

FBlockCPDInfo UBlockConfig::GetHighlightInfoByTag(const FGameplayTag& Tag) const
{
	if (const FBlockCPDInfo* Info = HighlightSettings.Find(Tag))
	{
		return *Info;
	}

	// 못 찾으면 기본값(0,0) 반환
	return FBlockCPDInfo();
}