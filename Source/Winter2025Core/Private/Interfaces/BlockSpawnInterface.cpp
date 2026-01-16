// Fill out your copyright notice in the Description page of Project Settings.


#include "BlockSpawnInterface.h"

// Add default functionality here for any IBlockSpawnInterface functions that are not pure virtual.

// 정적 변수 정의
TMap<TWeakObjectPtr<UWorld>, IBlockSpawnInterface*> IBlockSpawnInterface::SpawnerMap;

void IBlockSpawnInterface::RegisterSpawner(UWorld* World, IBlockSpawnInterface* Spawner)
{
    if (World && Spawner)
    {
        SpawnerMap.Add(World, Spawner);
        // 로그: 등록 확인 (개발 빌드에서만)
        UE_LOG(LogTemp, Verbose, TEXT("BlockSpawnInterface: Registered spawner for world %s"), *World->GetName());
    }
}

void IBlockSpawnInterface::UnregisterSpawner(UWorld* World)
{
    if (World)
    {
        SpawnerMap.Remove(World);
    }
}

IBlockSpawnInterface* IBlockSpawnInterface::GetBlockManagerSubsystem(const UWorld* World)
{
    if (!World)
    {
        return nullptr;
    }

    // TMap 검색은 평균 O(1) 시간 복잡도를 가짐
    IBlockSpawnInterface** FoundSpawner = SpawnerMap.Find(const_cast<UWorld*>(World));
    return FoundSpawner ? *FoundSpawner : nullptr;
}