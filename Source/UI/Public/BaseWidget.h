#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AbilitySystemComponent.h"
#include "BaseWidget.generated.h"

/**
 * [GAS UI 기본 클래스]
 * 이름 변경: WinterUserWidget -> BaseWidget
 * 모든 GAS 관련 위젯(체력바, 스킬슬롯 등)의 부모가 됩니다.
 */
UCLASS()
class UI_API UBaseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 외부(HUD 등)에서 ASC를 주입해주는 함수
	UFUNCTION(BlueprintCallable, Category = "GAS|UI")
	virtual void SetAbilitySystemComponent(UAbilitySystemComponent* InASC);

protected:
	// ASC가 설정된 직후 BP에서 추가 초기화를 하도록 알리는 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "GAS|UI")
	void OnAbilitySystemComponentSet();

	// 자식 위젯들이 사용할 ASC 포인터
	UPROPERTY(BlueprintReadOnly, Category = "GAS|UI")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
};