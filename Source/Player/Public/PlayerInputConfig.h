#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "PlayerInputConfig.generated.h"

/*
* IA와 GameplayTag를 매핑하는 구조체
* IA가 실행되면 해당하는 Tag 신호를 보내기 위함
* 초기화 시 한 번만 설정되므로 TArray로 구현
*/
USTRUCT(BlueprintType)
struct FPlayerInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	const UInputAction* InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly)
	FGameplayTag InputTag; // 예: Input.Ability.Slot1
};

UCLASS()
class UPlayerInputConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	// Q, E, R 키 등을 태그와 매핑한 목록
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FPlayerInputAction> AbilityInputActions;
};