// Fill out your copyright notice in the Description page of Project Settings.


#include "Block/ABlockBase.h"

// Sets default values
AABlockBase::AABlockBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	RootComponent = MeshComponent;
}

// Called when the game starts or when spawned
void AABlockBase::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AABlockBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AABlockBase::SpawnBlock(FVector SpawnLocation, EBlockType NewBlockType)
{
	Location = SpawnLocation;
	BlockType = NewBlockType;
	SetActorLocation(Location);
}

