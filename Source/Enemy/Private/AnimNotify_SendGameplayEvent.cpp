#include "AnimNotify_SendGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

FString UAnimNotify_SendGameplayEvent::GetNotifyName_Implementation() const
{
	// 에디터 트랙에 "GAS Event: 태그이름" 형태로 표시됨 (보기 편함)
	if (EventTag.IsValid())
	{
		return FString::Printf(TEXT("GAS Event: %s"), *EventTag.ToString());
	}
	return TEXT("Send GAS Event");
}

void UAnimNotify_SendGameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner()) return;

	// 1. 오너 가져오기
	AActor* OwnerActor = MeshComp->GetOwner();

	// 2. ASC(Ability System Component) 찾기
	// (UAbilitySystemBlueprintLibrary를 쓰면 안전하게 찾을 수 있음)
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor);

	// 3. ASC가 있을 때만 이벤트 발송 (에디터 미리보기에서는 ASC가 없으니 무시 -> 빨간 로그 해결!)
	if (ASC)
	{
		// 페이로드(데이터) 생성
		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.Instigator = OwnerActor;
		Payload.Target = OwnerActor;

		// 4. 이벤트 발사! (C++ 네이티브 호출이라 씹힐 확률 0%)
		// HandleGameplayEvent는 즉시 처리를 보장합니다.
		ASC->HandleGameplayEvent(EventTag, &Payload);
	}
}