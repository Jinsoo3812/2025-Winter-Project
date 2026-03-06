#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "Winter2025Player.generated.h"

class UPlayerInputConfig;
class UInputAction;
class UInputMappingContext;
struct FGameplayEventData;
struct FInputActionValue;
struct FOnAttributeChangeData;
class UNavigationInvokerComponent;

UCLASS()
class AWinter2025Player : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// ------------------
	// 초기화 함수
	// ------------------
	AWinter2025Player();
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	FGameplayTagContainer InitialGameplayTags;

protected:
	/*
	* 서버 - 클라이언트 공통 GAS 초기화 함수
	*/
	void InitializeGAS();

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UPlayerInputConfig> InputConfig;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	// ------------------
	// 네트워크 동기화
	// ------------------
public:
	// 애니메이션 블루프린트에서 가속도 대신 호출할 스레드 세이프 함수
	UFUNCTION(BlueprintPure, Category = "Movement|Animation", meta = (BlueprintThreadSafe))
	FVector GetProxySafeAcceleration() const;

	// ------------------
	// 입력 바인딩 처리 관련
	// IMC로 물리 키와 InputAction 매핑
	// ------------------
protected:
	// 이동/점프 전용 (WASD, Space) - Priority 0
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> BasicMappingContext;

	// 스킬 전용 (Q, E, R, 1, 2, 3 등) - Priority 1
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> SkillMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LeftClickAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MouseWheelClickAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	void Move(const FInputActionValue& Value);

	void OnMovementSpeedChanged(const FOnAttributeChangeData& Data);

	void OnLeftClick(const FInputActionValue& Value);

	void OnMouseWheelClick(const FInputActionValue& Value);

	// ------------------
	// 스킬 입력 처리 일반화
	// 태그 기반으로 어빌리티 활성화 시도
	// ------------------

	void Input_AbilityTagPressed(FGameplayTag InputTag);

	void Input_AbilityTagReleased(FGameplayTag InputTag);

	// ------------------
	// NavMesh 관련
	// ------------------
protected:
	// 런타임 NavMesh 생성을 유도하는 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation")
	TObjectPtr<UNavigationInvokerComponent> NavInvokerComp;

	UPROPERTY(EditDefaultsOnly, Category = "Navigation")
	float NavGenerationRadius = 3000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Navigation")
	float NavRemovalRadius = 5000.0f;
};
