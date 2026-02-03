// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "BlockCommonTypes.h"
#include "Previewtask.generated.h"

class USkillBase;
class IBlockSystemInterface;

// 스킬 범위의 모양 타입 정의
UENUM(BlueprintType)
enum class EPreviewShapeType : uint8
{
	Cylinder,
	Box,
	// 부채꼴 등 필요하면 추가
};

// 스킬마다 다른 범위를 정의하는 구조체
USTRUCT(BlueprintType)
struct FSkillPreviewRange
{
	GENERATED_BODY()

	// 모양 선택
	UPROPERTY(EditAnywhere, Category = "Preview")
	EPreviewShapeType ShapeType = EPreviewShapeType::Cylinder;

	// Box는 X, Y, Z를 모두 사용
	// Cylinder는 X를 반지름으로, Z를 높이로 사용
	UPROPERTY(EditAnywhere, Category = "Preview")
	FVector Dimensions = FVector(500.0f);

	// 플레이어를 기준으로 얼마나 떨어질지
	UPROPERTY(EditAnywhere, Category = "Preview")
	FVector RelativeOffset = FVector::ZeroVector;
};

/*
* 스킬들의 프리뷰 상태를 처리하는 Ability Task 클래스
*/
UCLASS()
class SKILL_API UPreviewTask : public UAbilityTask
{
	GENERATED_BODY()

public:
	/*
	* 프리뷰 Task 생성 함수
	* @param OwningAbility 이 Task를 소유할 Gameplay Ability
	* @param InRange TickTask에서 사용할 프리뷰 범위
	* @param InPreviewTag 프리뷰 범위 하이라이트에 사용할 Gameplay Tag
	* @param InCursorTag 커서 위치 하이라이트에 사용할 Gameplay Tag
	* @param VisualizerClass 범위 시각화에 사용할 액터 클래스 (기본값 nullptr)
	* @param bTraceCursor 마우스 커서 위치에 하이라이트 여부 (기본값 true)
	*/
	static UPreviewTask* CreatePreviewTask(
		USkillBase* OwningAbility,
		FSkillPreviewRange InRange,
		FGameplayTag InPreviewTag,
		FGameplayTag InCursorTag,
		TSubclassOf<AActor> InVisualizerClass = nullptr,
		bool InbHighlightCursorBlock = true
	);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

	/*
	* FSkillPreviewRange에 따라 매 프레임 블록 하이라이트 갱신
	*/
	virtual void TickTask(float DeltaTime) override;

	/* 입력받은 블록이 현재 프리뷰(범위) 내에 포함되어 있는지 확인 */
	bool IsBlockInPreview(const FBlockReference& BlockRef) const;

	//-------------------------------------------------------------
	// 프리뷰 범위 및 하이라이트
	// -------------------------------------------------------------
public:
	// 현재 범위 내에 들어와 하이라이트 되고 있는 모든 블록 반환
	const TArray<FBlockReference>& GetCurrentHighlightedBlocks() const { return HighlightedBlocks; }

	// 현재 마우스 커서가 가리키고 있는 블록 반환 (Tick에서 갱신됨)
	FBlockReference GetCurrentCursorBlock() const { return CurrentCursorBlock; }

protected:
	// 프리뷰 범위와 위치를 결정하는 구조체
	FSkillPreviewRange Range;

	// 프리뷰 범위 내 블록 하이라이트에 사용할 태그
	FGameplayTag PreviewTag;

	// 커서 위치의 블록을 하이라이트 할 지 여부
	bool bHighlightCursorBlock;

	// 커서 위치 블록 하이라이트에 사용할 태그
	FGameplayTag CursorTag;

	// 하이라이트 된 블록들
	TArray<FBlockReference> HighlightedBlocks;

	// 현재 커서가 가리키고 있는 블록
	FBlockReference CurrentCursorBlock;

	/*
	* 프리뷰 위치 계산, 시각화 액터 업데이트, 물리 충돌 검사 수행
	* @param OutOverlaps 결과로 받을 오버랩 배열
	* @param Avatar 캐릭터 (기준점)
	*/
	void UpdatePreviewShapeAndOverlap(TArray<FOverlapResult>& OutOverlaps, AActor* Avatar);

	// -------------------------------------------------------------
	// 시각화
	// -------------------------------------------------------------

	// 시각화 액터의 클래스
	TSubclassOf<AActor> VisualizerClass;

	// 시각화 액터의 인스턴스
	UPROPERTY()
	TObjectPtr<AActor> SpawnedVisualizer;

	// -------------------------------------------------------------
	// 캐싱
	// -------------------------------------------------------------

	/* BlockManagerSubsystem 캐싱 */
	IBlockSystemInterface* BlockSystem;
};
