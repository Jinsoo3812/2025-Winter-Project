// Fill out your copyright notice in the Description page of Project Settings.

#include "GE_BlockDestruction.h"

UGE_BlockDestruction::UGE_BlockDestruction()
{
	// GE가 즉시 적용 및 종료.
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 참고: UE 5.7에서는 InheritableGameplayEffectTags가 deprecated되었습니다.
	// 블루프린트에서 AssetTagsGameplayEffectComponent를 추가하고
	// "Block.Destruction" 태그를 Granted Tags에 설정하세요.
}
