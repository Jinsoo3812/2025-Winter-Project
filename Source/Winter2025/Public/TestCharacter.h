// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Winter2025Character.h"
#include "AbilitySystemInterface.h"
#include "Interface/ISkillManagerProvider.h"
#include "AbilitySystemComponent.h"
#include "TestCharacter.generated.h"

class UGameplayAbility;
class USkillManagerComponent;
class UInputMappingContext;
class UInputAction;

UCLASS()
/**
 * 서버는 클라이언트 모두 TestCharacter의 인스턴스를 생성하며
 * 서버는 권한 있는 진짜를 처리하고,
 * 클라이언트는 입력과 결과를 화면에 표시하는 역할을 담당한다.
 */
class WINTER2025_API ATestCharacter : public AWinter2025Character, public IAbilitySystemInterface, public ISkillManagerProvider
{
	GENERATED_BODY()

public:
	ATestCharacter();

	// IAbilitySystemInterface 구현
	// PlayerState의 ASC를 반환
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ISkillManagerProvider 인터페이스 구현
	// PlayerState에 캐싱된 SkillManager를 반환
	virtual USkillManagerComponent* GetSkillManager() const override { return CachedSkillManager; }

	// 입력 컴포넌트 설정
	// 로컬 플레이어의 입력을 바인딩함
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

	// 서버에서 컨트롤러가 해당 Pawn을 제어하기 시작할 때 호출되는 함수
	// 서버에서만 호출되며 PlayerState가 이미 할당된 상태임이 보장됨
	// PlayerState로부터 ASC와 SkillManager를 가져와 캐싱 및 초기화
	// @param NewController: 이 Pawn을 제어하는 새로운 컨트롤러
	virtual void PossessedBy(AController* NewController) override;

	// 서버가 클라이언트로의 PlayerState 복제를 완료한 후 호출되는 함수
	// 클라이언트에서만 호출되며 PlayerState가 이미 할당된 상태임이 보장됨
	// PlayerState로부터 ASC와 SkillManager를 가져와 캐싱 및 초기화
	virtual void OnRep_PlayerState() override;

	// 서버와 클라이언트 양쪽에서 호출되는 공통 초기화 함수
	// PlayerState로 부터 가져온 ASC와 SkillManager를 초기화
	void InitializeAbilitySystem();

	// 스킬 슬롯별 입력 핸들러 (InputID 기반)
	void OnAbilityInputPressed(int32 InputID);
	void OnAbilityInputReleased(int32 InputID);

protected:
	// PlayerState의 ASC 캐시 (빠른 접근을 위해)
	UPROPERTY(BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> CachedAbilitySystemComponent;

	// PlayerState의 SkillManager 캐시
	UPROPERTY(BlueprintReadOnly, Category = "Skill System")
	TObjectPtr<USkillManagerComponent> CachedSkillManager;

	// 초기화 완료 플래그
	bool bAbilitySystemInitialized = false;

	// Input Mapping Context 할당 변수
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	// 스킬 슬롯별 Input Action
	UPROPERTY(EditAnywhere, Category = "Input|Skills")
	TArray<TObjectPtr<UInputAction>> SkillSlotActions;
};
