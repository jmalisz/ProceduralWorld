// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "FastNoiseLite.h"
#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

#include "ErosionSimulator.h"

#include "NoiseGenerator.generated.h"

USTRUCT()
struct FChunkProperties
{
	GENERATED_BODY()

	FChunkProperties()
	{
	}

	// Chunk column number
	UPROPERTY()
	int ChunkNumberX = 0;

	// Chunk row number
	UPROPERTY()
	int ChunkNumberY = 0;

	UPROPERTY()
	UProceduralMeshComponent* TerrainMesh = nullptr;

	UPROPERTY()
	UProceduralMeshComponent* WaterMesh = nullptr;
};

UCLASS(BlueprintType, Blueprintable)
class PROCEDURALWORLD_API ANoiseGenerator : public AActor
{
	GENERATED_BODY()

public:
	ANoiseGenerator();

	UPROPERTY(EditAnywhere, Category="Noise settings")
	int GlobalOffsetX = 0;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	int GlobalOffsetY = 0;

	UPROPERTY(EditAnywhere, Category="Noise settings", Meta=(ClampMin=1, ClampMax=10))
	int Octaves = 5;

	UPROPERTY(EditAnywhere, Category="Noise settings", Meta=(ClampMin=1.f, ClampMax=20.f))
	float Lacunarity = 2.f;

	UPROPERTY(EditAnywhere, Category="Noise settings", Meta=(ClampMin=0.0001f))
	float NoiseScale = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mask settings")
	bool bApplyMask = false;

	UPROPERTY(EditAnywhere, Category="Mask settings")
	UCurveFloat* MoatHeightCurve = nullptr;

	UPROPERTY(EditAnywhere, Category="Erosion settings")
	bool bApplyErosion = true;

	UPROPERTY(EditAnywhere, Category="Map settings")
	bool bApplyRandomSeed = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map settings", Meta=(ClampMin=1, ClampMax=20))
	int MapSize = 3;

	UPROPERTY(EditAnywhere, Category="Map settings", Meta=(ClampMin=1))
	int MapSeed = 2021;

	UPROPERTY(EditAnywhere, Category="Map settings")
	UMaterialInstance* TerrainMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category="Map settings")
	UCurveFloat* TerrainHeightCurve = nullptr;

	UPROPERTY(EditAnywhere, Category="Map settings")
	UMaterialInstance* WaterMaterial = nullptr;

	UFUNCTION(BlueprintCallable)
	TArray<float> CreateNoiseData(float LocalOffsetX, float LocalOffsetY);

	UFUNCTION(BlueprintCallable)
	void UpdateGenerator();

	UFUNCTION(BlueprintCallable)
	TArray<float> CreateMask();

	UFUNCTION(BlueprintCallable)
	void GenerateTerrain(int TerrainIndex);

protected:
	// How many rendered squares per chunk, MapArraySize x MapArraySize
	int MapArraySize = 256;
	// Added border for edge normal calculation
	int EdgeArraySize = MapArraySize + 2;
	// Number of vertices/noise values
	UPROPERTY(BlueprintReadWrite)
	int NoiseArraySize = EdgeArraySize + 1;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Erosion settings")
	UErosionSimulator* ErosionSimulator;

private:
	UPROPERTY()
	TArray<FChunkProperties> World;
	TArray<float> Mask;

	FastNoiseLite NoiseGen;
	// Size of square made of 2 triangles
	float VertexSize = 100.f;
	// Multiplier for ThirdPerson module
	float HeightMultiplier = VertexSize * 10.f;

	void UpdateWorld();
	virtual void BeginPlay() override;
};
