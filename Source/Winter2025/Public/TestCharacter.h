// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Winter2025Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "TestCharacter.generated.h"

class UGameplayAbility;
class UInputAction;
class UInputMappingContext;

UCLASS()
class WINTER2025_API ATestCharacter : public AWinter2025Character, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ATestCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;

	// 플레이어 입력 컴포넌트 설정 함수야.
	// IMC 할당 및 IA 바인딩을 수행
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// TestCharacter의 ASC
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// TestCharacter가 보유한 GA 목록
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> Abilities;

	// GA를 발동을 바인딩할 Input Action
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AbilityInputAction;

	// Input Mapping Context
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

private:
	// GA 발동용 입력 처리 함수
	void OnAbilityInputPressed();
};
