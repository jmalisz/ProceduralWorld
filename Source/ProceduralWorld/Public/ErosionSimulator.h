// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ErosionSimulator.generated.h"

USTRUCT()
struct FGradientAndHeight
{
	GENERATED_BODY()

	FGradientAndHeight()
	{
	}

	float GradientX = 0.f;
	float GradientY = 0.f;
	float Height = 0.f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROCEDURALWORLD_API UErosionSimulator final : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UErosionSimulator();

	UFUNCTION(BlueprintCallable)
	void PrecalculateIndicesAndWeights();

	UFUNCTION(BlueprintCallable)
	void SimulateErosion(TArray<FVector>& HeightMap);

	UPROPERTY(EditAnywhere, Category="Erosion settings")
	bool bBlockBoundaryErosion = true;

	UPROPERTY(EditAnywhere, Category="Erosion settings", Meta=(ClampMin=2, ClampMax=20))
	int BorderSize = 3;

	UPROPERTY(EditAnywhere, Category="Erosion settings")
	bool bApplyBlur = true;

	UPROPERTY(EditAnywhere, Category="Erosion settings", Meta=(ClampMin=1.f, ClampMax=1000.f))
	float BaseWaterSpeed = 4.f;
	
	UPROPERTY(EditAnywhere, Category="Erosion settings", Meta=(ClampMin=0.f, ClampMax=1.f))
	float Inertia = 0.4f;

	UPROPERTY(EditAnywhere, Category="Erosion settings", Meta=(ClampMin=0.1f, ClampMax=1000.f))
	float SedimentCapacityFactor = 4.f;

	UPROPERTY(EditAnywhere, Category="Erosion settings", Meta=(ClampMin=0.f, ClampMax=10.f))
	float MinSedimentCapacity = 0.01f;

	UPROPERTY(EditAnywhere, Category="Erosion settings", Meta=(ClampMin=2, ClampMax=10))
	int ErosionRadius = 6;

	UPROPERTY(EditAnywhere, Category="Erosion settings", Meta=(ClampMin=0.f, ClampMax=1.f))
	float ErosionSpeed = 0.9f;

	UPROPERTY(EditAnywhere, Category="Erosion settings", Meta=(ClampMin=0.f, ClampMax=1.f))
	float DepositionSpeed = 0.1f;

	UPROPERTY(EditAnywhere, Category="Erosion settings", Meta=(ClampMin=0.f, ClampMax=1.f))
	float EvaporationSpeed = 0.1f;

	UPROPERTY(EditAnywhere, Category="Erosion settings", Meta=(ClampMin=1, ClampMax=100))
	int DropletLifetime = 30;

	UPROPERTY(EditAnywhere, Category="Erosion settings", Meta=(ClampMin=1, ClampMax=1000000))
	int IterationNumber = 70000;

	// Should be set up by parent
	int ChunkSize;
	float VertexSize;
	int ErosionSeed;
	
private:
	void GaussianBlur(TArray<FVector>& HeightMap);
	FGradientAndHeight* CalculateGradientAndHeight(TArray<FVector>& HeightMap, float RealPositionX, float RealPositionY);
	void DepositSediment(TArray<FVector>& HeightMap, int CombinedIndexPosition, float HeightDelta, float& Sediment, float SedimentCapacity );
	void ErodeTerrain(TArray<FVector>& HeightMap, int CombinedIndexPosition, float HeightDelta, float& Sediment, float SedimentCapacity);

	// Global indexes make simulation tracking easier
	int IterationIndex = 0;
	int DropletLifeIndex = 0;
	// An array of index offsets used for erosion
	TArray<TArray<int>> ErosionIndicesMap;
	// An array of weights applied to previous array of indexes
	TArray<TArray<float>> ErosionWeightsMap;
};
