#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h" // GAS 인터페이스 추가
#include "EnemyAttributeSet.h" // 적의 체력/스탯 데이터(AttributeSet) 클래스를 알기 위해 포함
#include "EnemyBase.generated.h"

/**
 * AEnemyBase
 * * [클래스 설명]
 * 게임에 등장하는 모든 적(Enemy)의 기본이 되는 부모 클래스입니다.
 * * [주요 역할]
 * 1. GAS(Gameplay Ability System) 컴포넌트를 보유하여 스킬 피격 및 효과 처리가 가능합니다.
 * 2. AttributeSet(속성 세트)을 보유하여 체력(Health) 등의 수치 데이터를 관리합니다.
 */
UCLASS()
class ENEMY_API AEnemyBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AEnemyBase();

	// IAbilitySystemInterface 구현: GAS 시스템이 이 캐릭터의 능력을 찾을 때 호출함
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	// [추가] 게임 시작 시 GAS 시스템을 초기화하기 위해 BeginPlay를 오버라이드합니다.
	virtual void BeginPlay() override;

protected:
	// ------------------------------------------------------------------------------------------
	// GAS (Gameplay Ability System) 관련 컴포넌트
	// ------------------------------------------------------------------------------------------

	/**
	 * [Ability System Component (ASC)]
	 * - 역할: 적이 스킬을 맞거나(피격), 버프/디버프 상태를 관리하는 'GAS의 심장' 역할을 합니다.
	 * - 적(AI)은 PlayerState가 없으므로, 이 컴포넌트를 자신의 몸체(Character)에 직접 지니고 있습니다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	class UAbilitySystemComponent* AbilitySystemComponent;

	/**
	 * [Attribute Set (속성 세트)]
	 * - 역할: 적의 '건강검진표'입니다. 체력(Health), 최대 체력(MaxHealth) 같은 데이터를 실제로 저장합니다.
	 * - 중요: UPROPERTY() 매크로가 없으면 언리얼 엔진이 "이거 안 쓰나 보네?" 하고 메모리에서 삭제해버릴 수 있습니다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	class UEnemyAttributeSet* Attributes;
};