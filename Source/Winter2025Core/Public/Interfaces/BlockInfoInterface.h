#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "BlockInfoInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UBlockInfoInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WINTER2025CORE_API IBlockInfoInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual FVector GetBlockLocation() const = 0;
	virtual FRotator GetBlockRotation() const = 0;
	virtual float GetBlockGridSize() const = 0;
	virtual FVector GetBlockAlignedLocation() const = 0;
};
