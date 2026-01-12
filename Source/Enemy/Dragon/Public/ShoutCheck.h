#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ShoutCheck.generated.h"

class UGameplayEffect;

/**
 * [ShoutCheck]
 * 포효(Shout) 도중, 드래곤 전방 부채꼴 범위 내에 있는 플레이어에게 디버프(이동속도 감소)를 겁니다.
 * - 특징: Z축(높이) 차이를 무시합니다.
 */
UCLASS()
class ENEMY_API UShoutCheck : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UShoutCheck();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

protected:
	// -------------------------------------------------------------------------
	// 에디터 설정 변수
	// -------------------------------------------------------------------------

	/** 기준이 될 소켓 이름 (예: MouthSocket) */
	UPROPERTY(EditAnywhere, Category = "Shout")
	FName SocketName;

	/** 부채꼴의 최대 거리 (반지름) */
	UPROPERTY(EditAnywhere, Category = "Shout")
	float ShoutRadius;

	/** 부채꼴의 각도 (예: 90도면 좌우 45도씩) */
	UPROPERTY(EditAnywhere, Category = "Shout", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float ShoutConeAngle;

	/** 적용할 디버프 (이동속도 감소 GE) */
	UPROPERTY(EditAnywhere, Category = "Shout")
	TSubclassOf<UGameplayEffect> DebuffEffect;

	/** 디버그용 부채꼴 그리기 */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug;

private:
	// 이번 포효에서 이미 당한 플레이어를 기록 (중복 적용 방지)
	UPROPERTY()
	TArray<AActor*> HitActors;
};
