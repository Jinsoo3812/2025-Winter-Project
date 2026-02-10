// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BlockConfig.h"
#include "BlockSettings.generated.h"


/**
 * 프로젝트 세팅(Project Settings) 창에 'Block System' 섹션을 추가하여
 * 전역 설정을 관리하는 클래스
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Block System"))
class WORLD_API UBlockSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	// 생성자에서 섹션 이름 등을 설정할 수 있음
	UBlockSettings();

	UPROPERTY(Config, EditAnywhere, Category = "Config", meta = (AllowedClasses = "/Script/World.BlockConfig"))
	TSoftObjectPtr<UBlockConfig> BlockConfigAsset;
};
