#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h" 
#include "GameplayTagContainer.h" // 태그 사용을 위해 추가
#include "EnemyBase.generated.h"

// 전방 선언 (헤더 의존성 최소화)
class UAbilitySystemComponent;
class UEnemyAttributeSet;
class UGameplayAbility;
class UGameplayEffect;

/**
 * [AEnemyBase]
 * 모든 적 캐릭터(보스, 쫄몹)의 최상위 부모 클래스.
 * GAS 시스템 초기화, 기본 스탯 설정, 시작 스킬 부여 기능을 담당합니다.
 */
UCLASS()
class ENEMY_API AEnemyBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AEnemyBase();

	// [IAbilitySystemInterface] 이 함수를 통해 외부에서 이 적의 ASC에 접근합니다.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 외부에서 적의 AttributeSet(체력 등)을 가져올 수 있게 Getter 제공
	UEnemyAttributeSet* GetAttributeSet() const { return Attributes; }

protected:
	virtual void BeginPlay() override;

	/**
	 * [GiveDefaultAbilities]
	 * 캐릭터 생성 시 에디터에 설정된 'StartupAbilities' 스킬들을 자동으로 부여합니다.
	 */
	virtual void GiveDefaultAbilities();

	/**
	 * [InitializeAttributes]
	 * 캐릭터 생성 시 'DefaultAttributeEffect'를 적용해 체력/마나 등의 초기값을 설정합니다.
	 */
	virtual void InitializeAttributes();

protected:
	// ------------------------------------------------------------------------------------------
	// GAS 컴포넌트
	// ------------------------------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UEnemyAttributeSet> Attributes;

	// ------------------------------------------------------------------------------------------
	// 설정 데이터 (에디터에서 기획자가 세팅)
	// ------------------------------------------------------------------------------------------

	/** 게임 시작 시 자동으로 부여할 스킬 목록 (예: 평타, 패시브) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** 게임 시작 시 스탯을 초기화할 GE (GameplayEffect) (예: 체력 1000 세팅) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Attributes")
	TSubclassOf<UGameplayEffect> DefaultAttributeEffect;

	/** 캐릭터가 기본적으로 가질 태그 (예: Enemy.Type.Boss) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Tags")
	FGameplayTagContainer InitialGameplayTags;
};