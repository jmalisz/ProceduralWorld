// Fill out your copyright notice in the Description page of Project Settings.


#include "ErosionSimulator.h"

UErosionSimulator::UErosionSimulator()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

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
					0.f, ErosionRadius - FMath::Sqrt(DistanceX * DistanceX + DistanceY * DistanceY)
				);

				if (x < 0 || x > ErosionMapSize || y < 0 || y > ErosionMapSize || DistanceWeight <= 0)
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

void UErosionSimulator::DepositSediment(TArray<FVector>& HeightMap, int CombinedIndexPosition, float SquareOffsetX,
                                        float SquareOffsetY, float HeightDelta, float& Sediment,
                                        float SedimentCapacity) const
{
	const float DepositAmount = HeightDelta < 0
		                            ? FMath::Min(abs(HeightDelta), Sediment)
		                            : (Sediment - SedimentCapacity) * DepositionSpeed;
	Sediment -= DepositAmount;

	HeightMap[CombinedIndexPosition].Z += DepositAmount * (1 - SquareOffsetX) * (1 - SquareOffsetY);
	HeightMap[CombinedIndexPosition + 1].Z += DepositAmount * SquareOffsetX * (1 - SquareOffsetY);
	HeightMap[CombinedIndexPosition + ErosionMapSize].Z += DepositAmount * (1 - SquareOffsetX) * SquareOffsetY;
	HeightMap[CombinedIndexPosition + 1 + ErosionMapSize].Z += DepositAmount * SquareOffsetX * SquareOffsetY;
}

void UErosionSimulator::ErodeTerrain(TArray<FVector>& HeightMap, int CombinedIndexPosition, float HeightDelta,
                                     float& Sediment, float SedimentCapacity) const
{
	const float ErosionAmount = FMath::Max(FMath::Min((SedimentCapacity - Sediment) * ErosionSpeed, HeightDelta), 0.f);

	for (int i = 0; i < ErosionIndicesMap[CombinedIndexPosition].Num(); i++)
	{
		const int ErodedVertex = ErosionIndicesMap[CombinedIndexPosition][i];
		const float WeightedErosionAmount = ErosionAmount * ErosionWeightsMap[CombinedIndexPosition][i];
		const float SedimentDelta = WeightedErosionAmount;

		HeightMap[ErodedVertex].Z -= SedimentDelta;
		Sediment += SedimentDelta;
	}
}


void UErosionSimulator::SimulateErosion(TArray<FVector>& HeightMap)
{
	// TArray<FVector> ErodedTerrain = HeightMap;
	const FRandomStream RandomStream(ErosionSeed);

	for (IterationIndex = 0; IterationIndex < IterationNumber; IterationIndex++)
	{
		float RealPositionX = RandomStream.FRandRange(0.f, ErosionMapSize - 1);
		float RealPositionY = RandomStream.FRandRange(0.f, ErosionMapSize - 1);
		float DirectionX = 0.f;
		float DirectionY = 0.f;
		float Speed = 1.f;
		float Water = 1.f;
		float Sediment = 0.f;

		for (DropletLifeIndex = 0; DropletLifeIndex < DropletLifetime; DropletLifeIndex++)
		{
			const int IndexPositionX = RealPositionX;
			const int IndexPositionY = RealPositionY;
			const int CombinedIndexPosition = IndexPositionX + IndexPositionY * ErosionMapSize;

			const float SquareOffsetX = RealPositionX - IndexPositionX;
			const float SquareOffsetY = RealPositionY - IndexPositionY;

			const FGradientAndHeight* CurrentGradientAndHeight = CalculateGradientAndHeight(
				HeightMap, RealPositionX, RealPositionY
			);

			DirectionX = DirectionX * Inertia - CurrentGradientAndHeight->GradientX * (1 - Inertia);
			DirectionY = DirectionY * Inertia - CurrentGradientAndHeight->GradientY * (1 - Inertia);

			// Normalize droplet direction
			const float CombinedDirection = FMath::Sqrt(DirectionX * DirectionX + DirectionY * DirectionY);

			if (CombinedDirection != 0.f)
			{
				DirectionX /= CombinedDirection;
				DirectionY /= CombinedDirection;
			}

			RealPositionX += DirectionX;
			RealPositionY += DirectionY;

			// Check if stopped in a pit or droplet flowed out of the map
			if (DirectionX == 0 && DirectionY == 0.f || RealPositionX < 0 || RealPositionX > ErosionMapSize - 1 ||
				RealPositionY < 0 || RealPositionY > ErosionMapSize - 1 || isnan(Speed))
				break;

			// Recalculate height at new position
			const FGradientAndHeight* NewGradientAndHeight = CalculateGradientAndHeight(
				HeightMap, RealPositionX, RealPositionY);

			const float HeightDelta = CurrentGradientAndHeight->Height - NewGradientAndHeight->Height;

			const float SedimentCapacity = FMath::Max(HeightDelta, MinSedimentCapacity) * Speed * Water *
				SedimentCapacityFactor;

			if (Sediment > SedimentCapacity || HeightDelta < 0)
				DepositSediment(HeightMap, CombinedIndexPosition, SquareOffsetX, SquareOffsetY, HeightDelta, Sediment,
				                SedimentCapacity);
			else
				ErodeTerrain(HeightMap, CombinedIndexPosition, HeightDelta, Sediment, SedimentCapacity);

			// Calculate droplet speed from potential and kinetic energy
			Speed = FMath::Sqrt(2 * Gravity * HeightDelta + Speed * Speed);
			Water *= 1 - EvaporationSpeed;
		}
	}
}
