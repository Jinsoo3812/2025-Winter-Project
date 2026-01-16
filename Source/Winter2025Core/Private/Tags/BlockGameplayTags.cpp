// Fill out your copyright notice in the Description page of Project Settings.


#include "Tags/BlockGameplayTags.h"

// Block Type Tags
UE_DEFINE_GAMEPLAY_TAG(Block_Type_Terrain, "Block.Type.Terrain");
UE_DEFINE_GAMEPLAY_TAG(Block_Type_Destructible, "Block.Type.Destructible");

// Block Highlight Tags
UE_DEFINE_GAMEPLAY_TAG(Block_Type_Destructible, "Block.Highlight.None");
UE_DEFINE_GAMEPLAY_TAG(Block_Type_Destructible, "Block.Highlight.Preview");
UE_DEFINE_GAMEPLAY_TAG(Block_Type_Destructible, "Block.Highlight.Bomb");

BlockGameplayTags::BlockGameplayTags()
{
}

BlockGameplayTags::~BlockGameplayTags()
{
}
