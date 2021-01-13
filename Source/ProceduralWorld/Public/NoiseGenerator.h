// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "FastNoiseLite.h"
#include "ProceduralMeshComponent.h"
#include "CoreMinimal.h"

#include "NoiseGenerator.generated.h"

UCLASS(BlueprintType, Blueprintable)
class PROCEDURALWORLD_API ANoiseGenerator : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ANoiseGenerator();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	int OffsetX = 0;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	int OffsetY = 0;

	UPROPERTY(EditAnywhere, Category="Noise settings", Meta=(ClampMin=1, ClampMax=10))
	int Octaves = 5;

	UPROPERTY(EditAnywhere, Category="Noise settings", Meta=(ClampMin=1.f, ClampMax=20.f))
	float Lacunarity = 2.f;

	UPROPERTY(EditAnywhere, Category="Noise settings", Meta=(ClampMin=0.0001f))
	float NoiseScale = 1.f;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	int Seed = 1337;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	bool bApplyRandomSeed = false;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	float VertexSize = 100.f;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	float HeightMultiplier = VertexSize * 10.f;

	UPROPERTY(EditAnywhere, Category="Terrain settings")
	UMaterialInstance* TerrainMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category="Terrain settings")
	UCurveFloat* HeightCurve = nullptr;

	UFUNCTION(BlueprintCallable)
	TArray<float> CreateNoiseData();

	UFUNCTION(BlueprintCallable)
	UTexture2D* CreateNoiseMap(TArray<float> NoiseArray);

	UFUNCTION(BlueprintCallable)
	void GenerateTerrain(TArray<float> NoiseArray);
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
private:
	UPROPERTY(VisibleAnywhere)
	UProceduralMeshComponent* Terrain = nullptr;

	FastNoiseLite NoiseGen;
	int MapTileWidth = 256;
	int MapTileHeight = 256;
	int NoiseArrayWidth = MapTileWidth + 1;
	int NoiseArrayHeight = MapTileHeight + 1;

	void UpdateGenerator();
	void RandomiseSeed();
};
