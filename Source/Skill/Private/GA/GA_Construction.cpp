// Fill out your copyright notice in the Description page of Project Settings.


#include "GA/GA_Construction.h"
#include "Block/DestructibleBlock.h"
#include "Block/BlockBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/OverlapResult.h"
#include "InputCoreTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Components/InputComponent.h"

UGA_Construction::UGA_Construction() {}

void UGA_Construction::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// LastPlayerLocation 초기화 (첫 실행 시 무조건 하이라이트 갱신되도록)
	LastPlayerLocation = FVector(FLT_MAX, FLT_MAX, FLT_MAX);

	// 프리뷰 업데이트를 위한 타이머 시작 (매 프레임)
	if (UWorld* World = GetWorld())
	{	
		// 60FPS 간격으로 UpdatePreview 함수 호출
		// 자식이 재정의한 UpdatePreview 또한 호출될 수 있음.
		World->GetTimerManager().SetTimer(TickTimerHandle, this, &UGA_Construction::UpdatePreview, 0.016f, true);
	}

	// WaitInputPress 어빌리티 태스크 생성
	WaitInputTask = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (WaitInputTask)
	{
		// OnPress 델리게이트에 콜백 함수(스킬 취소) 바인딩
		WaitInputTask->OnPress.AddDynamic(this, &UGA_Construction::OnCancelPressed);

		// 어빌리티 태스크 활성화
		WaitInputTask->ReadyForActivation();
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to create WaitInputTask"));
	}

	// 좌클릭 입력 바인딩
	// 좌클릭은 사용 범위가 넓고, 여러 어빌리티에서 공통으로 사용될 수 있으므로
	// Ability Task 대신 직접 InputComponent에 바인딩
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			// 좌클릭 키 바인딩 추가
			PC->InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &UGA_Construction::OnLeftClickPressed);
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("GA_Construction: PlayerController or InputComponent is null"));
		}
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: OwnerPawn is null"));
	}
}

void UGA_Construction::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickTimerHandle);
	}

	// 타이머 핸들 무효화
	TickTimerHandle.Invalidate();

	// LastPlayerLocation 리셋 (다음 활성화 시 하이라이트 갱신 보장)
	LastPlayerLocation = FVector::ZeroVector;

	// 하이라이트 제거
	ClearHighlights();

	// 프리뷰 블록 제거
	if (PreviewBlock)
	{
		PreviewBlock->Destroy();
		PreviewBlock = nullptr;
	}

	// Ability Task 정리
	if (WaitInputTask)
	{
		WaitInputTask->EndTask();
		WaitInputTask = nullptr;
	}

	// 좌클릭 바인딩 명시적 해제
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (OwnerPawn)
	{
		APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
		if (PC && PC->InputComponent)
		{
			// InputComponent에서 이 객체에 바인딩된 모든 키 바인딩 제거 (ASC를 사용하지 않았기에 수동 제거)
			for (int32 i = PC->InputComponent->KeyBindings.Num() - 1; i >= 0; --i)
			{
				if (PC->InputComponent->KeyBindings[i].KeyDelegate.GetUObject() == this)
				{
					PC->InputComponent->KeyBindings.RemoveAt(i);
				}
			}
		}
	}

	// 끝내는 함수는 자식이 먼저 호출하고, 마지막에 부모 함수 호출
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Construction::HighlightBlocksInRange()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: World is null"));
		return;
	}

	// AvatarActor를 OwnerPawn으로 캐시 
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: OwnerPawn is null"));
		return;
	}

	FVector PlayerLocation = OwnerPawn->GetActorLocation();

	// 박스 형태의 오버랩 영역 생성 (XY: 반지름, Z: 위아래 범위)
	// 원래 원통형 범위를 사용하고 싶지만, 언리얼 엔진의 오버랩 함수는 박스 형태만 지원하므로 박스로 대체
	// 이후에 XY 거리로 필터링하여 원형 범위로 보정
	FCollisionShape OverlapBox = FCollisionShape::MakeBox(FVector(ConstructionRangeXY, ConstructionRangeXY, ConstructionRangeZ));

	// 오버랩 결과를 저장할 배열
	TArray<FOverlapResult> OverlapResults;

	// 어떤 오브젝트 타입을 대상으로 충돌 쿼리를 수행할지 설정 (무엇을 찾을 지)
	FCollisionObjectQueryParams ObjectQueryParams;

	// 대부분의 경우 블록은 움직이지 않으므로 WorldStatic
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	// 추후 스킬 사용으로 블록이 움직이면, 그 위의 블록도 움직이는 등의 상황을 고려하여 WorldDynamic도 포함
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// 충돌 쿼리의 옵션 및 예외 설정 (어떻게 찾을 지)
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerPawn); // 플레이어 자신은 제외

	// 범위 내 블록만 검색 (언리얼 물리 엔진의 공간 분할 구조 활용)
	World->OverlapMultiByObjectType(
		OverlapResults,
		PlayerLocation,
		FQuat::Identity,
		ObjectQueryParams,
		OverlapBox,
		QueryParams
	);

	// 검색된 블록들 처리
	// FOverlapResult는 가벼우므로 &로 가져옵니다.
	// Pointer는 유효성 검사 및 메모리 접근 비용이 추가로 발생합니다.
	for (const FOverlapResult& Result : OverlapResults)
	{
		ABlockBase* Block = Cast<ABlockBase>(Result.GetActor());
		if (!Block) continue;

		FVector BlockLocation = Block->GetActorLocation();

		// XY 평면 거리 재확인 (박스가 사각형이므로 원형 범위로 한번 더 필터링)
		float DistanceXY = FVector::Dist2D(PlayerLocation, BlockLocation);
		if (DistanceXY > ConstructionRangeXY) continue;

		// 하이라이트 목록(그 위에 블록을 생성할 수 있는 블록들)에 추가
		HighlightedBlocks.Add(Block);

		// GetBlockMesh() 함수로 직접 메시 컴포넌트 가져오기
		UStaticMeshComponent* MeshComp = Block->GetBlockMesh();
		if (!MeshComp)
		{
			UE_LOG(LogTemp, Error, TEXT("GA_Construction: Block %s has no MeshComponent"), *Block->GetName());
			continue;
		}

		// 원본 머티리얼로부터 동적 머티리얼 인스턴스 생성 (중복 생성 방지)
		if (!DynamicMaterials.Contains(Block))
		{
			// 원본 머티리얼 가져오기
			UMaterialInterface* OriginalMaterial = MeshComp->GetMaterial(0);
			if (!OriginalMaterial)
			{
				UE_LOG(LogTemp, Error, TEXT("GA_Construction: Block %s has no material"), *Block->GetName());
				continue;
			}

			// 원본 머티리얼 저장
			OriginalMaterials.Add(Block, OriginalMaterial);

			// 동적 머티리얼 인스턴스 생성
			// UMaterialInstanceDynamic::Create 함수는 머티리얼 인스턴스를 생성하는 함수
			// @param OriginalMaterial: 원본 머티리얼
			// @param this: 소유자 객체 (여기서는 GA_Construction)
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(OriginalMaterial, this);
			if (!DynMat)
			{
				UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to create dynamic material for Block %s"), *Block->GetName());
				continue;
			}

			// 원본 EmissivePower 값 저장 (스칼라 파라미터가 없을 경우 0으로 처리)
			float OriginalEmissive = 0.0f;
			DynMat->GetScalarParameterValue(FName("EmissivePower"), OriginalEmissive);
			OriginalEmissivePowers.Add(Block, OriginalEmissive);

			// 동적 머티리얼 저장 및 메시에 적용
			DynamicMaterials.Add(Block, DynMat);
			MeshComp->SetMaterial(0, DynMat);
		}

		// 이미 동적 머티리얼이 생성된 블록이라면 EmissivePower 값만 업데이트
		UMaterialInstanceDynamic* DynMat = DynamicMaterials[Block];
		if (DynMat)
		{
			DynMat->SetScalarParameterValue(FName("EmissivePower"), HighlightEmissivePower);
		}
	}
}

void UGA_Construction::ClearHighlights()
{
	// 모든 하이라이트된 블록의 머티리얼 복구
	for (const TObjectPtr<ABlockBase>& Block : HighlightedBlocks)
	{
		if (!Block || !IsValid(Block.Get()))
		{
			UE_LOG(LogTemp, Error, TEXT("GA_Construction: Block is null or invalid during ClearHighlights"));
			continue;
		}

		UStaticMeshComponent* MeshComp = Block->GetBlockMesh();
		if (!MeshComp)
		{
			UE_LOG(LogTemp, Error, TEXT("GA_Construction: Block %s has no MeshComponent during ClearHighlights"), *Block->GetName());
			continue;
		}

		// 원본 머티리얼로 복구
		TObjectPtr<UMaterialInterface>* OriginalMat = OriginalMaterials.Find(Block.Get());
		if (OriginalMat)
		{
			MeshComp->SetMaterial(0, OriginalMat->Get());
		}
	}

	HighlightedBlocks.Empty();
	DynamicMaterials.Empty();
	OriginalMaterials.Empty();
	OriginalEmissivePowers.Empty();
}

void UGA_Construction::UpdatePreview()
{
	APawn* OwnerPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: OwnerPawn is null in UpdatePreview"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: PlayerController is null in UpdatePreview"));
		return;
	}

	// 매 프레임 플레이어 위치 기준으로 하이라이트 업데이트
	FVector CurrentPlayerLocation = OwnerPawn->GetActorLocation();
	if (!LastPlayerLocation.Equals(CurrentPlayerLocation, 10.0f)) // 10 이상 움직였을 때만
	{
		ClearHighlights(); // 기존 하이라이트 제거
		HighlightBlocksInRange(); // 새 위치 기준으로 재계산
		LastPlayerLocation = CurrentPlayerLocation;
	}

	// 마우스 커서 아래 블록 찾기
	FHitResult HitResult;
	PC->GetHitResultUnderCursor(ECC_Visibility, true, HitResult);

	// bBlockingHit은 Block 응답을 가진 충돌이 발생했는지 여부
	if (HitResult.bBlockingHit)
	{
		ABlockBase* HitBlock = Cast<ABlockBase>(HitResult.GetActor());
		
		// 하이라이트된 블록 중 하나인지 확인
		if (HitBlock && HighlightedBlocks.Contains(HitBlock))
		{
			// 프리뷰 블록이 없으면 생성 (BP에서 미리 디자인된 프리뷰 블록 사용)
			if (!PreviewBlock && PreviewBlockClass)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				
				PreviewBlock = GetWorld()->SpawnActor<AActor>(
					PreviewBlockClass, 
					FVector::ZeroVector, 
					FRotator::ZeroRotator, 
					SpawnParams
				);
				
				if (PreviewBlock)
				{
					// 충돌 비활성화
					PreviewBlock->SetActorEnableCollision(false);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to spawn PreviewBlock"));
				}
			}

			// 프리뷰 블록을 타겟 블록 위에 배치
			if (PreviewBlock)
			{
				FVector BlockLocation = HitBlock->GetActorLocation();
				FRotator BlockRotation = HitBlock->GetActorRotation();
				
				// 블록 크기만큼 위로 올림 (블록이 100x100x100이라 가정)
				FVector PreviewLocation = BlockLocation + FVector(0, 0, 100.0f);
				
				FCollisionQueryParams CheckParams;
				CheckParams.AddIgnoredActor(PreviewBlock);
				CheckParams.AddIgnoredActor(OwnerPawn); // 플레이어 충돌 제외

				// 블록 크기(50)보다 약간 작게(45) 설정하여 인접 블록과의 미세한 간섭 방지
				bool bIsOccupied = GetWorld()->OverlapBlockingTestByChannel(
					PreviewLocation,
					FQuat::Identity,
					ECC_WorldStatic,
					FCollisionShape::MakeBox(FVector(45.0f)),
					CheckParams
				);

				if (!bIsOccupied)
				{
					// 비어있는 공간이면 프리뷰 표시
					PreviewBlock->SetActorLocation(PreviewLocation);
					PreviewBlock->SetActorRotation(BlockRotation);
					PreviewBlock->SetActorHiddenInGame(false);
				}
				else
				{
					// 이미 자리에 블록이 있으면 숨김 처리
					PreviewBlock->SetActorHiddenInGame(true);
				}
			}
		}
		else
		{
			// 마우스 포인터가 가리키는 블록이 하이라이트된 블록이 아니면 프리뷰 숨김
			if (PreviewBlock)
			{
				PreviewBlock->SetActorHiddenInGame(true);
			}
		}
	}
	else
	{
		// 마우스 포인터가 가리키는 곳에서 Block 응답을 가진 충돌이 없으면 프리뷰 숨김
		if (PreviewBlock)
		{
			PreviewBlock->SetActorHiddenInGame(true);
		}
	}
}

void UGA_Construction::SpawnBlock()
{
	// Ability 활성화 커밋 (Cost, Cooldown 등 체크 및 적용)

	if (!CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to commit ability"));
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		return;
	}

	if (!PreviewBlock || PreviewBlock->IsHidden()) return;

	if (!BlockToSpawn)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: BlockToSpawn is null in SpawnBlock"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// 프리뷰 블록 위치에 실제 블록 생성
	FVector SpawnLocation = PreviewBlock->GetActorLocation();
	FRotator SpawnRotation = PreviewBlock->GetActorRotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADestructibleBlock* NewBlock = World->SpawnActor<ADestructibleBlock>(BlockToSpawn, SpawnLocation, SpawnRotation, SpawnParams);

	if (NewBlock)
	{
		// 블록 초기화 (필요한 경우)
		NewBlock->SpawnBlock(SpawnLocation, EBlockType::Destructible);
		
		UE_LOG(LogTemp, Log, TEXT("GA_Construction: Spawned new block %s at location %s"), *NewBlock->GetName(), *SpawnLocation.ToString());
		
		// 스킬 종료
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_Construction: Failed to spawn block"));
	}
}

void UGA_Construction::OnLeftClickPressed()
{
	// 프리뷰 블록이 존재하고, 숨겨져 있지 않을 때만 블록 생성 시도
	if (PreviewBlock && !PreviewBlock->IsHidden())
	{
		// 실제 스킬 시전 시작 알림
		// State.Busy 태그를 부여
		NotifySkillCastStarted();
		// 좌클릭 시 블록 생성 시도
		SpawnBlock();
	}
}

void UGA_Construction::OnCancelPressed(float TimeWaited)
{
	// W키 재입력 시 스킬 취소
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

