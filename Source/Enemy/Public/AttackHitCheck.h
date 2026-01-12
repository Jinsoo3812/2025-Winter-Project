#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AttackHitCheck.generated.h"

/**
 * [UAnimNotifyState_AttackHitCheck]
 * 애니메이션의 특정 구간 동안 공격 판정(Trace)을 수행하는 노티파이 스테이트입니다.
 * - 장점: 단순히 순간적인 판정이 아니라, 칼을 휘두르는 궤적을 따라 연속적으로 검사하므로 정확도가 높습니다.
 */
UCLASS()
class ENEMY_API UAnimNotifyState_AttackHitCheck : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_AttackHitCheck();

	// [애니메이션 시작 시 호출]
	// 판정에 필요한 초기 설정(무시할 액터 목록 초기화 등)을 수행합니다.
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	// [애니메이션 재생 중 매 프레임 호출]
	// 이전 프레임의 소켓 위치와 현재 위치 사이를 연결하는 'Sweep Trace'를 수행합니다.
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	// [애니메이션 종료 시 호출]
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	// -------------------------------------------------------------------------
	// 설정 변수 (에디터에서 기획자가 수정 가능)
	// -------------------------------------------------------------------------

	/** 공격 판정의 기준이 될 소켓 이름 (예: "RightHandSocket", "MouthSocket") */
	UPROPERTY(EditAnywhere, Category = "Attack")
	FName SocketName;

	/** 판정 구체(Sphere)의 반지름 */
	UPROPERTY(EditAnywhere, Category = "Attack")
	float AttackRadius;

	/** 디버그 드로잉을 켤지 여부 */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug;

private:
	// 이번 공격 동작 중에 이미 타격한 액터들을 저장합니다. (한 번 휘두를 때 같은 대상을 두 번 때리지 않기 위해)
	TArray<AActor*> HitActors;
};
