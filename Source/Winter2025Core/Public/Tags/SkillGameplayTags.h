

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill); // 스킬임을 표시하는 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Skill_Casting); // 스킬 시전 중임을 시전자에게 부여하는 태그
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Damage);   // 데미지 태그용
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Cooldown); // 쿨타임 태그용

// 페이즈 관리를 위한 태그 선언
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_BuffBarrier_Phase1); // 바닥에 범위를 표시한 상태
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_BuffBarrier_Phase2); // 블록을 소환한 상태

/**
 * 
 */
class WINTER2025CORE_API SkillGameplayTags
{
public:
	SkillGameplayTags();
	~SkillGameplayTags();
};
