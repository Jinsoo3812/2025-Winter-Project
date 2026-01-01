#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h" // GAS 인터페이스 추가
#include "EnemyBase.generated.h"

UCLASS()
class ENEMY_API AEnemyBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AEnemyBase();

	// IAbilitySystemInterface 구현: GAS 시스템이 이 캐릭터의 능력을 찾을 때 호출함
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	/** 적의 스탯과 능력을 관리하는 핵심 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	class UAbilitySystemComponent* AbilitySystemComponent;
};