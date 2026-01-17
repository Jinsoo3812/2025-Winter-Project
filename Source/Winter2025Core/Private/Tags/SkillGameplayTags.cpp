#include "SkillGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Skill, "Skill");
UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Casting, "State.Casting");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage, "Data.Skill.Damage");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Cooldown, "Data.Skill.Cooldown");

// 페이즈 관리를 위한 태그 정의
UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_BuffBarrier_Phase1, "State.Skill.BuffBarrier.Phase1");
UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_BuffBarrier_Phase2, "State.Skill.BuffBarrier.Phase2");

SkillGameplayTags::SkillGameplayTags()
{
}

SkillGameplayTags::~SkillGameplayTags()
{
}
