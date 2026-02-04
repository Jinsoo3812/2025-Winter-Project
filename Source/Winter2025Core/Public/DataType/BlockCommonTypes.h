

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BlockCommonTypes.generated.h"

class AChunkBase;

// 블록의 종류를 정의하는 Enum
UENUM(BlueprintType)
enum class EBlockType : uint8
{
	None 	UMETA(DisplayName = "None"), // 빈 공간
	Terrain		UMETA(DisplayName = "Terrain"),
	Destructible	UMETA(DisplayName = "Destructible")
};

// 6방향 이웃 블록을 나타내는 Enum
enum class EBlockNeighbor : uint8
{
	Front = 0,  // X + 1
	Back,       // X - 1
	Right,      // Y + 1
	Left,       // Y - 1
	Up,         // Z + 1
	Down,       // Z - 1
	Count
};

// 청크 내의 블록 1개의 상태를 저장하는 경량 구조체
USTRUCT(BlueprintType)
struct FBlockData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBlockType Type = EBlockType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Health = 100.0f;

	UPROPERTY()
	bool bIsActorSpawned = false;

	// 청크 내에는 수많은 블록이 존재하므로, 메모리 패딩을 고려하여 멤버를 추가할 것
};

// Actor 블록과 HISM 블록을 구분하지 않고 참조하기 위한 구조체
USTRUCT(BlueprintType)
struct FBlockReference
{
	GENERATED_BODY()

	// 청크 액터 혹은 개별 블록 액터
	TWeakObjectPtr<UObject> TargetObject = nullptr;

	// 충돌 검사에서 걸린 컴포넌트 (HISM Component)
	TWeakObjectPtr<UPrimitiveComponent> TargetComponent = nullptr;

	// HISM 인덱스 (개별 액터인 경우 -1)
	int32 ItemIndex = -1;

	FBlockReference()
		: TargetObject(nullptr), TargetComponent(nullptr), ItemIndex(-1) {
	}

	void Reset()
	{
		TargetObject = nullptr;
		TargetComponent = nullptr;
		ItemIndex = -1;
	}

	bool IsValid() const { return TargetObject.IsValid(); }

	// == 연산자 오버로딩 (TArray.Find 등에서 사용)
	bool operator==(const FBlockReference& Other) const
	{
		return TargetObject.Get() == Other.TargetObject.Get() &&
			TargetComponent.Get() == Other.TargetComponent.Get() &&
			ItemIndex == Other.ItemIndex;
	}

	friend uint32 GetTypeHash(const FBlockReference& Ref)
	{
		uint32 Hash = GetTypeHash(Ref.TargetObject.Get());
		Hash = HashCombine(Hash, GetTypeHash(Ref.TargetComponent.Get()));
		Hash = HashCombine(Hash, GetTypeHash(Ref.ItemIndex));
		return Hash;
	}
};

/*
* 블록의 CPD 인덱스와 변경 값을 담는 구조체
* CPDIndex: 수정할 CPD 인덱스
* CPDValue: CPD 인덱스의 변경 값
*/
USTRUCT(BlueprintType)
struct FBlockCPDInfo
{
	GENERATED_BODY()

	// 수정할 CPD 인덱스
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 CPDIndex = 0;

	// CPD 인덱스의 변경 값
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CPDValue = 0.0f;
};

/**
 * 단일 블록의 정의
 * 모든 블록이 공통으로 가지되, 그 타입별로 다르게 가지는 속성들
 */
USTRUCT(BlueprintType)
struct FBlockDefinition
{
	GENERATED_BODY()

	// 블록 타입
	UPROPERTY(EditAnywhere, Category = "Definition")
	EBlockType Type = EBlockType::None;

	// 블록 메시
	UPROPERTY(EditAnywhere, Category = "Definition")
	UStaticMesh* Mesh = nullptr;

	// Actor 여부
	UPROPERTY(EditAnywhere, Category = "Definition")
	bool bIsActor = false;

	// 블록의 Gameplay Tag
	UPROPERTY(EditAnywhere, Category = "Definition", meta = (EditCondition = "bIsActor"))
	FGameplayTag Tag;

	// 블록의 Class 타입
	UPROPERTY(EditAnywhere, Category = "Definition", meta = (EditCondition = "bIsActor"))
	TSubclassOf<AActor> ActorClass;
};

