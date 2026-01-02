#pragma once

#include "CoreMinimal.h"
#include "Winter2025Character.h"    // [중요] 이동/카메라 기능이 있는 부모 클래스
#include "AbilitySystemInterface.h" // GAS 시스템과 소통하기 위한 인터페이스
#include "PlayerAttributeSet.h"     // 같은 Player 폴더에 있는 체력 데이터 클래스
#include "PlayerBase.generated.h"

/**
 * APlayerBase
 * ACharacter (언리얼 기본) -> AWinter2025Character (탑다운 이동, 카메라 기능 포함)
 * -> APlayerBase (GAS 전투 시스템 추가)
 * * [주요 역할]
 * 1. 부모(Winter2025Character)에게서 물려받은 이동 기능을 그대로 사용합니다.
 * 2. GAS(Ability System)를 장착하여 스킬을 쓰고, 체력(HP)을 가집니다.
 * 3. 나중에 전사, 마법사 같은 직업이 생긴다면 이 클래스를 상속받아 만듭니다.
 */
UCLASS()
class WINTER2025_API APlayerBase : public AWinter2025Character, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// 생성자: 컴포넌트(부품)들을 조립하는 곳
	APlayerBase();

	// [IAbilitySystemInterface 구현]
	// 외부 시스템이 "너 GAS 컴포넌트 어딨어?" 라고 물어볼 때 호출되는 함수입니다.
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:

	virtual void BeginPlay() override;

	// ------------------------------------------------------------------------------------------
	// GAS (Gameplay Ability System) 컴포넌트
	// ------------------------------------------------------------------------------------------

	/**
	 * [Ability System Component (ASC)]
	 * - 역할: 플레이어의 '스킬 엔진'입니다. 스킬 실행, 쿨타임, 버프 관리를 총괄합니다.
	 * - VisibleAnywhere: 에디터 상세 창에서 볼 수 있음.
	 * - BlueprintReadOnly: 블루프린트에서 가져다 쓸 수 있음.
	 * - AllowPrivateAccess: private/protected 변수여도 에디터에서 접근 허용.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	class UAbilitySystemComponent* AbilitySystemComponent;

	/**
	 * [Attribute Set (속성 세트)]
	 * - 역할: 플레이어의 '스탯 통'입니다. 체력(Health), 마나(Mana) 등의 데이터가 들어갑니다.
	 * - UPROPERTY가 없으면 언리얼이 메모리 관리를 안 해서 게임 도중 데이터가 증발할 수 있습니다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	class UPlayerAttributeSet* Attributes;
};