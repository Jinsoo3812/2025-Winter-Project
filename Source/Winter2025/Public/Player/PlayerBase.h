#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"   //부모 클래스
#include "AbilitySystemInterface.h" // GAS 시스템과 소통하기 위한 인터페이스
#include "PlayerBase.generated.h"

class UCameraComponent;
class USpringArmComponent;


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
class WINTER2025_API APlayerBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// 생성자: 컴포넌트(부품)들을 조립하는 곳
	APlayerBase();

	// [인터페이스 유지] 
	// 내 몸에 ASC가 없더라도, 연결된 PlayerState의 ASC를 찾아서 리턴해줘야 합니다.
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

private:

	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	// 중복 코드를 방지하기 위해 GAS 초기화 로직을 별도 함수로 뺐습니다.
	void InitAbilityActorInfo();

protected:

	/*virtual void BeginPlay() override;*/


	// [서버용] 컨트롤러가 빙의(Possess)했을 때 호출됩니다.
	virtual void PossessedBy(AController* NewController) override;

	// [클라이언트용] PlayerState가 서버로부터 복제되어 내게 도착했을 때 호출됩니다.
	virtual void OnRep_PlayerState() override;

public:
	/** Update */
	virtual void Tick(float DeltaSeconds) override;

	/** Returns the camera component **/
	UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent.Get(); }

	/** Returns the Camera Boom component **/
	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }
};