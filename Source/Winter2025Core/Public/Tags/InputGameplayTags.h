

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Input과 관련된 GameplayTags를 정의하는 클래스
 */
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Input_LeftClick);

WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Input_Skill_Slot1);
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Input_Skill_Slot2);
WINTER2025CORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Input_Skill_Slot3);

/**
 * 
 */
class WINTER2025CORE_API InputGameplayTags
{
public:
	InputGameplayTags();
	~InputGameplayTags();
};
