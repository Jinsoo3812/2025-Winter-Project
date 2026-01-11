#include "Dragon_RushCheck.h"
#include "BossDragon.h" // BossDragon의 함수를 호출해야 하므로 헤더 포함 필수

UDragon_RushCheck::UDragon_RushCheck()
{
	// 에디터에서 노티파이를 봤을 때 구분하기 쉬운 색상으로 설정 (선택 사항)
	// 예: 붉은색 계열
	NotifyColor = FColor::Red;
}

void UDragon_RushCheck::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// 1. 메쉬와 메쉬의 주인(Owner Actor)이 존재하는지 확인 (안전장치)
	if (MeshComp && MeshComp->GetOwner())
	{
		// 2. 주인을 ABossDragon 클래스로 형변환(Cast) 합니다.
		// 이유: SetRushCollisionEnabled 함수는 부모(EnemyBase)가 아닌 자식(BossDragon)에만 있기 때문입니다.
		if (ABossDragon* Dragon = Cast<ABossDragon>(MeshComp->GetOwner()))
		{
			// 3. 박스 콜리전 활성화 (ON)
			// 이제부터 플레이어가 박스에 닿으면 데미지를 입습니다.
			Dragon->SetRushCollisionEnabled(true);
		}
	}
}

void UDragon_RushCheck::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// 1. 메쉬와 주인 확인
	if (MeshComp && MeshComp->GetOwner())
	{
		// 2. 다시 형변환
		if (ABossDragon* Dragon = Cast<ABossDragon>(MeshComp->GetOwner()))
		{
			// 3. 박스 콜리전 비활성화 (OFF)
			// 돌진이 끝났으므로 닿아도 데미지를 입지 않게 합니다.
			Dragon->SetRushCollisionEnabled(false);
		}
	}
}