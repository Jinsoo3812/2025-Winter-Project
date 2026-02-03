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
	*/
	static UPreviewTask* CreatePreviewTask(
		USkillBase* OwningAbility,
		FSkillPreviewRange InRange,
		FGameplayTag InPreviewTag,
		FGameplayTag InCursorTag
	);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

	/*
	* Range를 반지름으로 하는 원형의 주변 블록들을 하이라이트
	* 마우스 커서 아래의 블록은 별도 하이라이트
	*/
	virtual void TickTask(float DeltaTime) override;

	/* 입력받은 블록이 현재 프리뷰(범위) 내에 포함되어 있는지 확인 */
	bool IsBlockInPreview(const FBlockReference& BlockRef) const;

protected:
	FSkillPreviewRange Range;

	FGameplayTag PreviewTag;
	FGameplayTag CursorTag;

	// 이전에 하이라이트된 블록들을 기억했다가 끄기 위함
	TArray<FBlockReference> PreviousHighlights;

	/* BlockManagerSubsystem 캐싱 */
	IBlockSystemInterface* BlockSystem;
};
