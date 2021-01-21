#pragma once

#include "ProceduralMeshComponent.h"

#include "ChunkProperties.generated.h"

USTRUCT()
struct FChunkProperties
{
	GENERATED_BODY()
	
	FChunkProperties(){}

	// Vertex X and Y size, generally shouldn't be made lower than 100.f
	UPROPERTY(EditAnywhere, Category="Terrain settings")
	float VertexSize = 100.f;

	// Required to make game looks normal for character model, 10 times Vertex size seems to work best
	UPROPERTY(EditAnywhere, Category="Terrain settings")
	float HeightMultiplier = VertexSize * 10.f;
	
	UPROPERTY(EditAnywhere, Category="Terrain settings")
	UMaterialInstance* TerrainMaterial = nullptr;
	
	UPROPERTY(EditAnywhere, Category="Terrain settings")
	UCurveFloat* HeightCurve = nullptr;
	
	// Chunk column number
	UPROPERTY()
	int ChunkNumberX = 0;
	
	// Chunk row number
	UPROPERTY()
	int ChunkNumberY = 0;
	
	UPROPERTY()
	UProceduralMeshComponent* TerrainMesh = nullptr;
};
