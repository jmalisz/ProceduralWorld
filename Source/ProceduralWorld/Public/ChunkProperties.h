#pragma once

#include "ProceduralMeshComponent.h"

#include "ChunkProperties.generated.h"

USTRUCT()
struct FChunkProperties
{
	GENERATED_BODY()

	FChunkProperties()
	{
	}

	UPROPERTY(EditAnywhere, Category="World settings")
	UMaterialInstance* TerrainMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category="World settings")
	UMaterialInstance* WaterMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category="World settings")
	UCurveFloat* HeightCurve = nullptr;

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
