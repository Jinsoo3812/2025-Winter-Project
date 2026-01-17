// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Block과 관련된 GameplayTags를 정의하는 클래스
 */

 // Block Type Tags
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Type_Terrain);
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Type_Destructible);

// Block Highlight Tags
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_None);
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_Preview);
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_Target);
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_Bomb);
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_Bomb_None);

class WINTER2025CORE_API BlockGameplayTags
{
public:
	BlockGameplayTags();
	~BlockGameplayTags();
};
