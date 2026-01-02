// Fill out your copyright notice in the Description page of Project Settings.


#include "Block/BlockBase.h"

// Sets default values
ABlockBase::ABlockBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	RootComponent = MeshComponent;

	// 기본 Cube 메시 로드
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
	{
		DefaultBlockMesh = CubeMesh.Object;
	}
}

// Called when the game starts or when spawned
void ABlockBase::BeginPlay()
{
	Super::BeginPlay();

	// DefaultBlockMesh가 설정되어 있으면 MeshComponent에 적용
	if (DefaultBlockMesh && MeshComponent)
	{
		MeshComponent->SetStaticMesh(DefaultBlockMesh);
	}
}

// Called every frame
void ABlockBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABlockBase::SpawnBlock(FVector SpawnLocation, EBlockType NewBlockType)
{
	Location = SpawnLocation;
	BlockType = NewBlockType;
	SetActorLocation(Location);
}

