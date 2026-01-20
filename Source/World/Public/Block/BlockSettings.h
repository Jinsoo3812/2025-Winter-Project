// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Block/DA_BlockConfig.h"
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

	// 데이터 에셋을 지정할 프로퍼티
	// TSoftObjectPtr을 사용하여 에셋이 필요할 때 로드되도록 유도 (메모리 관리 측면에서 유리)
	UPROPERTY(Config, EditAnywhere, Category = "Config", meta = (AllowedClasses = "/Script/World.DA_BlockConfig"))
	TSoftObjectPtr<UDA_BlockConfig> BlockConfigAsset;
};
