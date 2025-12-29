// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_BlockDestruction.generated.h"

UCLASS()
class WINTER2025_API UGE_BlockDestruction : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// Block.Destruction 태그를 대상에게 부여하는 GE의 생성자.
	UGE_BlockDestruction();
};
