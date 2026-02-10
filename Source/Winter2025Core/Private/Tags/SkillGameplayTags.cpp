#include "SkillGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Skill, "Skill");
UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Casting, "State.Casting");
UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_Preview, "Player.State.Preview");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage, "Data.Skill.Damage");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Cooldown, "Data.Skill.Cooldown");

// 페이즈 관리를 위한 태그 정의
UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_BuffBarrier_Phase1, "State.Skill.BuffBarrier.Phase1");
UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_BuffBarrier_Phase2, "State.Skill.BuffBarrier.Phase2");

UE_DEFINE_GAMEPLAY_TAG(TAG_Player_State_Preview, "Player.State.Preview");
UE_DEFINE_GAMEPLAY_TAG(TAG_Skill_State_Preview, "Skill.State.Preview");

SkillGameplayTags::SkillGameplayTags()
{
}

SkillGameplayTags::~SkillGameplayTags()
{
}
