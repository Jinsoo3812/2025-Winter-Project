// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlockPreviewInterface.h"
#include "PreviewActorBase.generated.h"

class IBlockSystemInterface;
class UBlockConfig;

UCLASS()
class WORLD_API APreviewActorBase : public AActor, public IBlockPreviewInterface
{
	GENERATED_BODY()

	// -----------------------------------------------------------------------------
	// 초기화 및 기본 정의
	// -----------------------------------------------------------------------------
public:	
	APreviewActorBase();

protected:
	void BeginPlay() override;

	// 위치를 갖는 기본 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* RootScene;

	// HISM 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UHierarchicalInstancedStaticMeshComponent* HISMComponent;

	// HISM 인스턴스 초기화 (한 번만 수행)
	UFUNCTION(BlueprintCallable, Category = "Preview")
	void RebuildInstances();

	// -----------------------------------------------------------------------------
	// 프리뷰 블록 생성 관련
	// -----------------------------------------------------------------------------
public:
	/*
	 * 매 틱마다 호출되어 프리뷰 블록을 갱신하는 함수
	 * @param TargetLocation: 마우스 커서가 가리키는 월드 좌표 (스냅된 위치)
	 * @return bool: 커서가 가리키는 TargetLocation이 설치 가능한지 여부
	 */
	virtual bool UpdatePreviewState(FVector TargetLocation) override;

	/*
	 * 건설 확정 시 호출되는 함수
	 * @return TArray<FTransform> : 소환할 가능한 블록들의 월드 좌표 리스트
	 */
	virtual TArray<FTransform> GetValidSpawnData() override;

protected:
	// 생성 블록들의 상대적 위치 배열
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview")
	TArray<FVector> RelativeBlockOffsets;

	// 현재 건설 가능한 HISM 인스턴스 인덱스들
	TSet<int32> ValidInstanceIndices;


	// -----------------------------------------------------------------------------
	// 캐싱
	// -----------------------------------------------------------------------------
protected:
	// BlockManagerSubsystem 캐시
	IBlockSystemInterface* BlockSystem;

	UPROPERTY(Transient)
	// BlockConfig 캐시
	TObjectPtr<UBlockConfig> BlockConfig;
};
