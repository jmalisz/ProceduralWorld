// 

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Visualization.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROCEDURALWORLD_API UVisualization : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static UTexture2D* CreateNoiseTexture(TArray<float> NoiseArray, float NoiseArraySize);

	UFUNCTION(BlueprintCallable)
	static UTexture2D* CreateMaskTexture(TArray<float> MaskArray, float MapSideLength);
};
