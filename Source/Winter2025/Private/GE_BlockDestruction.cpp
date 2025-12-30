// Fill out your copyright notice in the Description page of Project Settings.

#include "GE_BlockDestruction.h"

UGE_BlockDestruction::UGE_BlockDestruction()
{
	// GE가 즉시 적용 및 종료.
	DurationPolicy = EGameplayEffectDurationType::Instant;
}
