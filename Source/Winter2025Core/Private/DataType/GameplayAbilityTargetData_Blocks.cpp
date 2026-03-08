// Fill out your copyright notice in the Description page of Project Settings.


#include "GameplayAbilityTargetData_Blocks.h"

bool FGameplayAbilityTargetData_Blocks::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	// 1. 배열 크기 최적화 직렬화 (가변 비트 정수: 값이 작으면 1바이트만 소모)
	uint32 BlockCount = Blocks.Num();
	Ar.SerializeIntPacked(BlockCount);

	// 패킷을 수신(Loading)하는 서버 입장이라면, 미리 배열 메모리를 할당
	if (Ar.IsLoading())
	{
		Blocks.SetNumZeroed(BlockCount);
	}

	// 2. 블록 순회 직렬화
	for (uint32 i = 0; i < BlockCount; ++i)
	{
		FBlockReference& Ref = Blocks[i];

		// HISM 여부를 단 1비트로 압축 (ItemIndex가 0 이상이면 HISM)
		uint32 bIsHISMBit = (Ref.ItemIndex >= 0) ? 1 : 0;
		Ar.SerializeBits(&bIsHISMBit, 1);
		bool bIsHISM = (bIsHISMBit == 1);

		if (bIsHISM)
		{
			// [HISM 블록 패킹]
			UObject* CompObj = Ref.TargetComponent.Get();

			// Ar << 연산자는 내부적으로 UPackageMap을 통해 UObject를 NetGUID로 자동 압축 변환함
			Ar << CompObj;

			// 음수일 리 없으므로 uint32로 캐스팅 후 가변 길이 정수로 압축
			uint32 Index = (uint32)FMath::Max(0, Ref.ItemIndex);
			Ar.SerializeIntPacked(Index);

			// 수신(Loading) 측 복원 로직
			if (Ar.IsLoading())
			{
				Ref.TargetComponent = Cast<UPrimitiveComponent>(CompObj);
				Ref.ItemIndex = (int32)Index;

				// 핵심 최적화: 서버는 Component만 받아서 Owner(Chunk Actor)를 스스로 유추함!
				if (Ref.TargetComponent.IsValid())
				{
					Ref.TargetObject = Ref.TargetComponent->GetOwner();
				}
				else
				{
					Ref.TargetObject = nullptr;
				}
			}
		}
		else
		{
			// [Actor 블록 패킹]
			UObject* ActorObj = Ref.TargetObject.Get();

			// HISM이 아니므로 액터 자체의 NetGUID만 전송
			Ar << ActorObj;

			// 수신(Loading) 측 복원 로직
			if (Ar.IsLoading())
			{
				Ref.TargetObject = ActorObj;
				Ref.TargetComponent = nullptr; // 보통 루트 컴포넌트를 직접 특정할 필요는 없으므로 생략
				Ref.ItemIndex = -1; // Actor임을 나타내는 -1 고정
			}
		}
	}

	bOutSuccess = true;
	return true;
}