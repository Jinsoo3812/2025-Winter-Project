// Fill out your copyright notice in the Description page of Project Settings.


#include "Tags/BlockGameplayTags.h"

// Block Type Tags
UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Type_Terrain, "Block.Type.Terrain");
UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Type_Destructible, "Block.Type.Destructible");

// Block Highlight Tags
UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Highlight_None, "Block.Highlight.None");
UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Highlight_Preview, "Block.Highlight.Preview");
UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Highlight_Target, "Block.Highlight.Target");
UE_DEFINE_GAMEPLAY_TAG(TAG_Block_Highlight_Bomb, "Block.Highlight.Bomb");

BlockGameplayTags::BlockGameplayTags()
{
}

BlockGameplayTags::~BlockGameplayTags()
{
}
