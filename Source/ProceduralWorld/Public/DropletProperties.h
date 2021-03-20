#pragma once

#include "DropletProperties.generated.h"

USTRUCT()
struct FDropletProperties
{
	GENERATED_BODY()

	FDropletProperties()
	{
	}

	// Position on heightmap grid
	UPROPERTY()
	int GridPositionX = 0;

	UPROPERTY()
	int GridPositionY = 0;
	
	// Last direction taken, -1 means unset
	UPROPERTY()
	int LastDirection = -1;

	// Current droplet speed
	UPROPERTY()
	float Speed = 0;

	// Size of carried sediment. Cannot be higher than water.
	UPROPERTY()
	float Sediment = 0;

	// Size of remaining water in droplet
	UPROPERTY()
	float Water = 0;
};
