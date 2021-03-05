// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "FastNoiseLite.h"
#include "CoreMinimal.h"

#include "ChunkProperties.h"


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
	int GlobalOffsetX = 0;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	int GlobalOffsetY = 0;

	UPROPERTY(EditAnywhere, Category="Noise settings", Meta=(ClampMin=1, ClampMax=10))
	int Octaves = 5;

	UPROPERTY(EditAnywhere, Category="Noise settings", Meta=(ClampMin=1.f, ClampMax=20.f))
	float Lacunarity = 2.f;

	UPROPERTY(EditAnywhere, Category="Noise settings", Meta=(ClampMin=0.0001f))
	float NoiseScale = 0.2f;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	int Seed = 1337;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	bool bApplyRandomSeed = false;

	UPROPERTY(EditAnywhere, Category="Map settings")
	bool bApplyFalloffMap = false;

	UPROPERTY(EditAnywhere, Category="Map settings", Meta=(ClampMin=1, ClampMax=20))
	int MapSize = 3;

	UPROPERTY(EditAnywhere, Category="Map settings")
	UMaterialInstance* DefaultTerrainMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category="Map settings")
	UCurveFloat* DefaultHeightCurve = nullptr;

	UPROPERTY(EditAnywhere, Category="Map settings")
	UMaterialInstance* DefaultWaterMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category="Map settings")
	UCurveFloat* SeaHeightCurve = nullptr;

	UFUNCTION(BlueprintCallable)
	TArray<float> CreateNoiseData(float LocalOffsetX, float LocalOffsetY);

	UFUNCTION(BlueprintCallable)
	TArray<float> CreateFalloffMap();

	UFUNCTION(BlueprintCallable)
	UTexture2D* CreateNoiseTexture(TArray<float> NoiseArray);

	UFUNCTION(BlueprintCallable)
	UTexture2D* CreateFalloffTexture(TArray<float> FalloffArray);

	UFUNCTION(BlueprintCallable)
	void GenerateTerrain(int TerrainIndex);
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
private:
	UPROPERTY(VisibleAnywhere)
	TArray<FChunkProperties> World;
	TArray<float> WorldMap;

	FastNoiseLite NoiseGen;
	FCriticalSection ActorMutex;
	float VertexSize = 100.f;
	float HeightMultiplier = VertexSize * 10.f;
	// How many rendered squares per chunk, MapArraySize x MapArraySize
	int MapArraySize = 256;
	// Added border for edge normal calculation
	int EdgeArraySize = MapArraySize + 2;
	// Number of vertices/noise values
	int NoiseArraySize = EdgeArraySize + 1;

	void UpdateWorld();
};
