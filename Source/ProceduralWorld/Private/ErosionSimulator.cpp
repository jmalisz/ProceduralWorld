// Fill out your copyright notice in the Description page of Project Settings.


#include "ErosionSimulator.h"

UErosionSimulator::UErosionSimulator()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

// Initialization function, called before BeginPlay(). Calculates erosion indices and weights.
void UErosionSimulator::InitializeComponent()
{
	// Total number of squares in map
	const int TotalMapSize = ErosionMapSize * ErosionMapSize;
	// Number of unique vertices for a given erosion radius
	const int VerticesNumber = ErosionRadius * ErosionRadius * 4;

	TArray<FIntPoint> VertexOffsets;
	TArray<float> VertexWeights;

	ErosionIndicesMap.Reserve(TotalMapSize);
	ErosionWeightsMap.Reserve(TotalMapSize);
	VertexOffsets.Reserve(VerticesNumber);
	VertexWeights.Reserve(VerticesNumber);

	// Loop over every vertex on map
	for (int i = 0; i < TotalMapSize; i++)
	{
		const int ErosionCentreX = i % ErosionMapSize;
		const int ErosionCentreY = i / ErosionMapSize;

		float WeightedSum = 0.f;

		for (int y = ErosionCentreY - ErosionRadius; y <= ErosionCentreY + ErosionRadius; y++)
		{
			for (int x = ErosionCentreX - ErosionRadius; x <= ErosionCentreX + ErosionRadius; x++)
			{
				const float DistanceX = ErosionCentreX - x, DistanceY = ErosionCentreY - y;
				const float DistanceWeight = FMath::Max(
					0.f, 1 - FMath::Sqrt(DistanceX * DistanceX + DistanceY * DistanceY) / ErosionRadius
				);

				if (x < 0 || x > ErosionMapSize - 1 || y < 0 || y > ErosionMapSize - 1 || DistanceWeight == 0.f)
					continue;

				VertexOffsets.Add(FIntPoint(x, y));
				VertexWeights.Add(DistanceWeight);
				WeightedSum += DistanceWeight;
			}
		}

		ErosionIndicesMap.AddDefaulted();
		ErosionIndicesMap[i].Init(0, VertexOffsets.Num());

		ErosionWeightsMap.AddDefaulted();
		ErosionWeightsMap[i].Init(0.f, VertexOffsets.Num());

		for (int j = 0; j < VertexOffsets.Num(); j++)
		{
			ErosionIndicesMap[i][j] = VertexOffsets[j].X + VertexOffsets[j].Y * ErosionMapSize;
			ErosionWeightsMap[i][j] = VertexWeights[j] / WeightedSum;
		}
		
		VertexOffsets.Reset();
		VertexWeights.Reset();
	}
}

// Applies blur using mean filter
void UErosionSimulator::GaussianBlur(TArray<FVector>& HeightMap)
{
	// Total number of squares in map
	const int TotalMapSize = ErosionMapSize * ErosionMapSize;
	const TArray<FVector> HeightMapCopy = HeightMap;

	// Loop over every vertex on map
	for (int CombinedIndex = 0; CombinedIndex < TotalMapSize; CombinedIndex++)
	{
		const int IndexX = CombinedIndex % ErosionMapSize;
		const int IndexY = CombinedIndex / ErosionMapSize;
		float NewValue = 0.f;

		for (int y = IndexY - 1; y <= IndexY + 1; y++)
		{
			for (int x = IndexX - 1; x <= IndexX + 1; x++)
			{
				// Using regular box blur
				if (x < 0 || x > ErosionMapSize - 1 || y < 0 || y > ErosionMapSize - 1)
				{
					NewValue += HeightMapCopy[CombinedIndex].Z;
					continue;
				}
				NewValue += HeightMapCopy[x + y * ErosionMapSize].Z;
			}
		}
		HeightMap[CombinedIndex].Z = NewValue / 9.f;
	}
}

// Calculates gradient and height of current point inside vertex square
FGradientAndHeight* UErosionSimulator::CalculateGradientAndHeight(TArray<FVector>& HeightMap, float RealPositionX,
                                                                  float RealPositionY)
{
	FGradientAndHeight* GradientAndHeight = new FGradientAndHeight;
	const int IndexPositionX = RealPositionX;
	const int IndexPositionY = RealPositionY;

	const float SquareOffsetX = RealPositionX - IndexPositionX;
	const float SquareOffsetY = RealPositionY - IndexPositionY;

	// Get square vertices heights
	const int CombinedIndexPosition = IndexPositionX + IndexPositionY * ErosionMapSize;
	const float HeightNW = HeightMap[CombinedIndexPosition].Z;
	const float HeightNE = HeightMap[CombinedIndexPosition + 1].Z;
	const float HeightSW = HeightMap[CombinedIndexPosition + ErosionMapSize].Z;
	const float HeightSE = HeightMap[CombinedIndexPosition + 1 + ErosionMapSize].Z;

	GradientAndHeight->GradientX = (HeightNE - HeightNW) * (1 - SquareOffsetY) + (HeightSE - HeightSW) * SquareOffsetY;
	GradientAndHeight->GradientY = (HeightSW - HeightNW) * (1 - SquareOffsetX) + (HeightSE - HeightNE) * SquareOffsetX;

	GradientAndHeight->Height = HeightNW * (1 - SquareOffsetX) * (1 - SquareOffsetY) + HeightNE * SquareOffsetX * (1 -
		SquareOffsetY) + HeightSW * (1 - SquareOffsetX) * SquareOffsetY + HeightSE * SquareOffsetX * SquareOffsetY;

	return GradientAndHeight;
}

// Deposits water droplet sediment based on parameters
void UErosionSimulator::DepositSediment(TArray<FVector>& HeightMap, int CombinedIndexPosition, float HeightDelta,
                                        float& Sediment, float SedimentCapacity)
{
	const float DepositAmount = HeightDelta > 0
		                            ? FMath::Min(HeightDelta, Sediment)
		                            : (Sediment - SedimentCapacity) * DepositionSpeed;
	Sediment -= DepositAmount;

	HeightMap[CombinedIndexPosition].Z += DepositAmount * 0.25f;
	HeightMap[CombinedIndexPosition + 1].Z += DepositAmount * 0.25f;
	HeightMap[CombinedIndexPosition + ErosionMapSize].Z += DepositAmount * 0.25f;
	HeightMap[CombinedIndexPosition + 1 + ErosionMapSize].Z += DepositAmount * 0.25f;
}

// Erodes terrain and gathers sediment to droplet
void UErosionSimulator::ErodeTerrain(TArray<FVector>& HeightMap, int CombinedIndexPosition, float HeightDelta,
                                     float& Sediment, float SedimentCapacity)
{
	const float ErosionAmount = FMath::Max(FMath::Min((SedimentCapacity - Sediment) * ErosionSpeed, -HeightDelta), 0.f);

	// Uses precalculated vertices for each vertex on the map
	for (int i = 0; i < ErosionIndicesMap[CombinedIndexPosition].Num(); i++)
	{
		const int ErodedVertex = ErosionIndicesMap[CombinedIndexPosition][i];

		// Applies boundaries to each map chunk in order to keep seams between them
		if ((ErodedVertex / ErosionMapSize < BorderSize || ErodedVertex / ErosionMapSize > ErosionMapSize - (BorderSize
			+ 1) || ErodedVertex % ErosionMapSize < BorderSize || ErodedVertex % ErosionMapSize > ErosionMapSize - (
			BorderSize + 1)) && bBlockBoundaryErosion)
			continue;

		const float WeightedErosionAmount = ErosionAmount * ErosionWeightsMap[CombinedIndexPosition][i];
		const float SedimentDelta = WeightedErosionAmount;

		HeightMap[ErodedVertex].Z -= SedimentDelta;
		Sediment += SedimentDelta;
	}
}

// Main function, responsible for simulating droplet erosion
void UErosionSimulator::SimulateErosion(TArray<FVector>& HeightMap)
{
	// TArray<FVector> ErodedTerrain = HeightMap;
	const FRandomStream RandomStream(ErosionSeed);

	for (IterationIndex = 0; IterationIndex < IterationNumber; IterationIndex++)
	{
		float RealPositionX = RandomStream.FRandRange(ErosionRadius, ErosionMapSize - ErosionRadius);
		float RealPositionY = RandomStream.FRandRange(ErosionRadius, ErosionMapSize - ErosionRadius);
		float DirectionX = 0.f;
		float DirectionY = 0.f;
		float Speed = BaseWaterSpeed;
		float Water = 1.f;
		float Sediment = 0.f;

		for (DropletLifeIndex = 0; DropletLifeIndex < DropletLifetime; DropletLifeIndex++)
		{
			const int IndexPositionX = RealPositionX;
			const int IndexPositionY = RealPositionY;
			const int CombinedIndexPosition = IndexPositionX + IndexPositionY * ErosionMapSize;

			const FGradientAndHeight* CurrentGradientAndHeight = CalculateGradientAndHeight(
				HeightMap, RealPositionX, RealPositionY
			);

			// Calculate direction of fastest descent
			DirectionX = DirectionX * Inertia - CurrentGradientAndHeight->GradientX * (1 - Inertia);
			DirectionY = DirectionY * Inertia - CurrentGradientAndHeight->GradientY * (1 - Inertia);

			// Normalize droplet direction
			const float CombinedDirection = FMath::Max(
				0.01f, FMath::Sqrt(DirectionX * DirectionX + DirectionY * DirectionY));
			DirectionX /= CombinedDirection;
			DirectionY /= CombinedDirection;
			
			RealPositionX += DirectionX;
			RealPositionY += DirectionY;

			// Check if droplet stopped in a pit or flowed out of the map chunk (or entered chunk border)
			if (DirectionX == 0.f && DirectionY == 0.f || (RealPositionX < BorderSize || RealPositionX > ErosionMapSize
				- (BorderSize + 1) || RealPositionY < BorderSize || RealPositionY > ErosionMapSize - (BorderSize + 1
				)) && bBlockBoundaryErosion)
				break;

			// Recalculate height at new position
			const FGradientAndHeight* NewGradientAndHeight = CalculateGradientAndHeight(
				HeightMap, RealPositionX, RealPositionY);
			
			const float HeightDelta = NewGradientAndHeight->Height - CurrentGradientAndHeight->Height;

			const float SedimentCapacity = FMath::Max(-HeightDelta, MinSedimentCapacity) * Speed *
				Water * SedimentCapacityFactor;

			if (Sediment > SedimentCapacity || HeightDelta > 0)
				DepositSediment(HeightMap, CombinedIndexPosition, HeightDelta, Sediment, SedimentCapacity);
			else
				ErodeTerrain(HeightMap, CombinedIndexPosition, HeightDelta, Sediment, SedimentCapacity);

			// Calculate droplet speed as an approximation based on slope
			Speed = FMath::Max(-HeightDelta * BaseWaterSpeed, 0.f);
			Water *= 1 - EvaporationSpeed;
		}
	}
	if (bApplyBlur) GaussianBlur(HeightMap);
}
