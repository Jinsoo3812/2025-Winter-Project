


#include "BlockSystemInterface.h"

// 정적 멤버 변수 정의
TMap<TWeakObjectPtr<UWorld>, IBlockSystemInterface*> IBlockSystemInterface::GlobalSystemMap;

void IBlockSystemInterface::RegisterSystem(UWorld* World, IBlockSystemInterface* System)
{
	if (World && System)
	{
		GlobalSystemMap.Add(World, System);
	}
}

void IBlockSystemInterface::UnregisterSystem(UWorld* World)
{
	if (World)
	{
		GlobalSystemMap.Remove(World);
	}
}

IBlockSystemInterface* IBlockSystemInterface::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) {
		UE_LOG(LogTemp, Warning, TEXT("BlockSystemInterface: WorldContextObject is null."));
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) {
		UE_LOG(LogTemp, Warning, TEXT("BlockSystemInterface: Unable to get UWorld from WorldContextObject."));
		return nullptr;
	}

	// 맵에서 해당 월드에 등록된 시스템을 찾아서 반환
	if (IBlockSystemInterface** FoundSystem = GlobalSystemMap.Find(World))
	{
		return *FoundSystem;
	}

	return nullptr;
}
