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
	float NoiseScale = 1.f;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	int Seed = 1337;

	UPROPERTY(EditAnywhere, Category="Noise settings")
	bool bApplyRandomSeed = false;

	UPROPERTY(EditAnywhere, Category="Map settings")
	int MapSizeX = 3;

	UPROPERTY(EditAnywhere, Category="Map settings")
	int MapSizeY = 3;

	UPROPERTY(EditAnywhere, Category="Map settings")
	UMaterialInstance* DefaultMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category="Map settings")
	UCurveFloat* DefaultHeightCurve = nullptr;

	UFUNCTION(BlueprintCallable)
	TArray<float> CreateNoiseData(float LocalOffsetX, float LocalOffsetY);

	UFUNCTION(BlueprintCallable)
	UTexture2D* CreateNoiseMap(TArray<float> NoiseArray);

	UFUNCTION(BlueprintCallable)
	void GenerateTerrain(int TerrainIndex);
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
private:
	UPROPERTY(VisibleAnywhere)
	TArray<FChunkProperties> World;

	FastNoiseLite NoiseGen;
	FCriticalSection ActorMutex;
	int MapArrayWidth = 256;
	int MapArrayHeight = 256;
	// To create a map with 256 squares, we need 257x257 noise values
	int NoiseArrayWidth = MapArrayWidth + 1;
	int NoiseArrayHeight = MapArrayHeight + 1;

	void UpdateGenerator();
	void RandomiseSeed();
	void SetUpChunk(int TerrainIndex);
};
