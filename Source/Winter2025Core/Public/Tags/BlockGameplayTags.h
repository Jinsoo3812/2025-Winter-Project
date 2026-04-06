// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Block과 관련된 GameplayTags를 정의하는 파일
 */

 // Block Type Tags
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Type_Terrain);
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Type_Destructible);

// Block Highlight Tags
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_Preview);
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_Target);
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_Bomb_None);
// ���� ���� ���� �±�
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_AttackZone);
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_AttackZone_None);

// 진짜 쓰는 것들만
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight); // Block.Highlight
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_None); // Block.Highlight.None
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_Bomb); // Block.Highlight.Bomb
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_Bomb_None); // Block.Highlight.Bomb.None
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_Invalid); // Block.Highlight.Invalid
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Block_Highlight_Warning); // Block.Highlight.Warning
