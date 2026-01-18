#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Winter2025Player.generated.h"

class UPlayerInputConfig;
class UInputAction;
class UInputMappingContext;
struct FGameplayEventData;
struct FInputActionValue;
struct FOnAttributeChangeData;

UCLASS()
class AWinter2025Player : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AWinter2025Player();
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	void InitializeGAS(); // 공통 초기화 로직

	// 입력을 처리하는 일반화된 함수
	void Input_AbilityTagPressed(FGameplayTag InputTag);
	void Input_AbilityTagReleased(FGameplayTag InputTag);

	// AttributeSet의 이벤트를 처리할 콜백
	void HandleAttributeEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UPlayerInputConfig> InputConfig;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	// Input Mapping Context 할당 변수
	// [기본] 이동/점프 전용 (WASD, Space) - Priority 0
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> BasicMappingContext;

	// [스킬] 스킬 전용 (Q, E, R, 1, 2, 3 등) - Priority 1
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> SkillMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LeftClickAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	void Move(const FInputActionValue& Value);

	void OnMovementSpeedChanged(const FOnAttributeChangeData& Data);
};