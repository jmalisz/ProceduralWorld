// Fill out your copyright notice in the Description page of Project Settings.


#include "NoiseGenerator.h"

#include <tuple>


#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values for this component's properties
ANoiseGenerator::ANoiseGenerator()
{
	const int ChunksNumber = MapSizeX * MapSizeY;

	PrimaryActorTick.bCanEverTick = false;

	NoiseGen.SetFractalType(FastNoiseLite::FractalType_FBm);
	NoiseGen.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);

	World.Reserve(ChunksNumber);

	for (int y = 0; y < MapSizeY; y++)
	{
		for (int x = 0; x < MapSizeX; x++)
		{
			const FName ObjectName = *FString::Printf(TEXT("TerrainMesh%i"), x + y * MapSizeY);
			World.Add(FChunkProperties());
			World[x + y * MapSizeY].TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(ObjectName);
			World[x + y * MapSizeY].ChunkNumberX = x;
			World[x + y * MapSizeY].ChunkNumberY = y;
			World[x + y * MapSizeY].TerrainMesh->bUseAsyncCooking = true;
		}
	}
	RootComponent = World[0].TerrainMesh;
}


TArray<float> ANoiseGenerator::CreateNoiseData(float LocalOffsetX, float LocalOffsetY)
{
	const float HalfNoiseArrayWidth = NoiseArrayWidth / 2;
	const float HalfNoiseArrayHeight = NoiseArrayHeight / 2;

	TArray<float> NoiseData;

	NoiseData.Reserve(NoiseArrayWidth * NoiseArrayHeight);
	UpdateGenerator();

	if (NoiseScale <= 0)
		NoiseScale = 0.0001f;

	for (int y = 0; y < NoiseArrayHeight; y++)
	{
		for (int x = 0; x < NoiseArrayWidth; x++)
		{
			const float SampleX = (x + LocalOffsetX - HalfNoiseArrayWidth) * NoiseScale + GlobalOffsetX;
			const float SampleY = (y + LocalOffsetY - HalfNoiseArrayHeight) * NoiseScale + GlobalOffsetY;
			const float NoiseValue = NoiseGen.GetNoise(SampleX, SampleY);

			NoiseData.Add((NoiseValue + 1) / 2);
		}
	}

	return NoiseData;
}

UTexture2D* ANoiseGenerator::CreateNoiseMap(TArray<float> NoiseArray)
{
	UTexture2D* NoiseMap = UTexture2D::CreateTransient(NoiseArrayWidth, NoiseArrayHeight);
	TArray<FColor> ColorMap;

	ColorMap.Reserve(NoiseArrayWidth * NoiseArrayHeight);

	for (int y = 0; y < NoiseArrayHeight; y++)
	{
		for (int x = 0; x < NoiseArrayWidth; x++)
		{
			const int Grayscale = FMath::Lerp(0, 255, NoiseArray[x + y * NoiseArrayHeight]);
			ColorMap.Add(FColor(Grayscale, Grayscale, Grayscale));
		}
	}

	if (!NoiseMap)
		return nullptr;

#if WITH_EDITORONLY_DATA
	NoiseMap->MipGenSettings = TMGS_NoMipmaps;
#endif
	NoiseMap->NeverStream = true;
	NoiseMap->SRGB = 0;

	FTexture2DMipMap& Mip = NoiseMap->PlatformData->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);

	FMemory::Memcpy(Data, ColorMap.GetData(), ColorMap.GetAllocatedSize());
	Mip.BulkData.Unlock();
	NoiseMap->UpdateResource();

	return NoiseMap;
}

void ANoiseGenerator::GenerateTerrain(int TerrainIndex)
{
	const FChunkProperties* WorldHandle = &World[TerrainIndex];

	// Get required data from struct
	UProceduralMeshComponent* Terrain = WorldHandle->TerrainMesh;
	const float HeightMultiplier = WorldHandle->HeightMultiplier;
	const UCurveFloat* HeightCurve = WorldHandle->HeightCurve;
	UMaterialInstance* TerrainMaterial = WorldHandle->TerrainMaterial;
	const float VertexSize = WorldHandle->VertexSize;
	const float ChunkOffsetX = WorldHandle->ChunkNumberX * MapArrayWidth;
	const float ChunkOffsetY = WorldHandle->ChunkNumberY * MapArrayHeight;

	// Data for procedural mesh
	TArray<float> NoiseArray = CreateNoiseData(ChunkOffsetX, ChunkOffsetY);
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UV;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor> Colors;
	//Starting position for chunk
	const float StartingPositionX = WorldHandle->ChunkNumberX ? ChunkOffsetX * VertexSize : 0;
	const float StartingPositionY = WorldHandle->ChunkNumberY ? ChunkOffsetY * VertexSize : 0;
	const float UVStartingPositionX = WorldHandle->ChunkNumberX ? ChunkOffsetX * 1.f : 0;
	const float UVStartingPositionY = WorldHandle->ChunkNumberY ? ChunkOffsetY * 1.f : 0;

	if (!World[TerrainIndex].HeightCurve)
	{
		UE_LOG(LogTemp, Error, TEXT("Terrain generation: HeightCurve not set"));
		return;
	}

	// The numbers are number of times array is accessed inside loop
	Vertices.Reserve(NoiseArrayWidth * NoiseArrayHeight);
	Triangles.Reserve(6 * MapSizeX * MapSizeX);
	UV.Reserve(NoiseArrayWidth * NoiseArrayHeight);
	// Normals.Reserve(NoiseArrayWidth * NoiseArrayHeight);
	Normals.Init(FVector(0.f), NoiseArrayWidth * NoiseArrayHeight);
	Tangents.Reserve(NoiseArrayWidth * NoiseArrayHeight);
	Colors.Reserve(NoiseArrayWidth * NoiseArrayHeight);

	UE_LOG(LogTemp, Warning, TEXT("Terrain generation thread calculation started: MeshName - %s"), *Terrain->GetName());

	/* Mesh building schematic. First triangle is TL->BL->TR, second one is TR->BL->BR.
	 * TL---TR x++
	 * |  /  |
	 * BL---BR
	 * y++;
	 */

	// First double loop allocates Vertex coordinate. There are 2 loops because triangles need immediate access to vertices.
	for (int y = 0; y < NoiseArrayHeight; y++)
	{
		for (int x = 0; x < NoiseArrayWidth; x++)
		{
			const int Height = HeightMultiplier * HeightCurve->GetFloatValue(NoiseArray[x + y * NoiseArrayHeight]);
			Vertices.Add(FVector(StartingPositionX + VertexSize * x, StartingPositionY + VertexSize * y, Height));
			UV.Add(FVector2D(UVStartingPositionX + x * 1.0f, UVStartingPositionY + y * 1.0f));
		}
	}

	// Second double loop combines correct vertices into triangles. 
	for (int y = 0; y < MapArrayHeight; y++)
	{
		for (int x = 0; x < MapArrayWidth; x++)
		{
			// Smooth normals calculations
			// Vertex vectors are named after their value
			const FVector VertexX = Vertices[x + y * NoiseArrayHeight];
			const FVector VertexXp1 = Vertices[x + 1 + y * NoiseArrayHeight];
			const FVector VertexYp1 = Vertices[x + (y + 1) * NoiseArrayHeight];
			const FVector VertexXYp1 = Vertices[x + 1 + (y + 1) * NoiseArrayHeight];

			const FVector CrossProduct1 = FVector::CrossProduct(VertexXp1 - VertexX, VertexYp1 - VertexX);
			const FVector CrossProduct2 = FVector::CrossProduct(VertexXp1 - VertexYp1, VertexXYp1 - VertexYp1);

			Normals[x + y * NoiseArrayHeight] += CrossProduct1;
			Normals[x + 1 + y * NoiseArrayHeight] += CrossProduct1;
			Normals[x + 1 + y * NoiseArrayHeight] += CrossProduct2;
			Normals[x + (y + 1) * NoiseArrayHeight] += CrossProduct1;
			Normals[x + (y + 1) * NoiseArrayHeight] += CrossProduct2;
			Normals[x + 1 + (y + 1) * NoiseArrayHeight] += CrossProduct2;
			
			// TL
			Triangles.Add(x + y * NoiseArrayHeight);
			// BL
			Triangles.Add(x + (y + 1) * NoiseArrayHeight);
			// TR
			Triangles.Add(x + 1 + y * NoiseArrayHeight);
			// TR
			Triangles.Add(x + 1 + y * NoiseArrayHeight);
			// BL
			Triangles.Add(x + (y + 1) * NoiseArrayHeight);
			// BR
			Triangles.Add(x + 1 + (y + 1) * NoiseArrayHeight);
		}
	}

	for (int i = 0; i < NoiseArrayWidth * NoiseArrayHeight; i++)
	{
		Normals[i].Normalize();
		if (!Normals[i].IsNormalized())
		UE_LOG(LogTemp, Warning, TEXT("Normal is not normalized"));
	}

	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		Terrain->CreateMeshSection(0, Vertices, Triangles, Normals, UV, Colors, Tangents, true);
		Terrain->SetMaterial(0, TerrainMaterial);
		// ReSharper disable once CppExpressionWithoutSideEffects
		Terrain->ContainsPhysicsTriMeshData(true);
	});

	UE_LOG(LogTemp, Warning, TEXT("Terrain generation thread completed: %s"), *Terrain->GetName());
}

// Called when the game starts
void ANoiseGenerator::BeginPlay()
{
	// Super::BeginPlay();

	if (bApplyRandomSeed)
		RandomiseSeed();

	if (!World.GetData())
	{
		UE_LOG(LogTemp, Error, TEXT("Terrain generation: HeightCurve not set"));
		return;
	}

	for (int i = 0; i < MapSizeX * MapSizeY; i++)
	{
		AsyncTask(ENamedThreads::HighTaskPriority, [&, i]()
		{
			SetUpChunk(i);
			GenerateTerrain(i);
		});
	}
}

void ANoiseGenerator::UpdateGenerator()
{
	NoiseGen.SetFractalOctaves(Octaves);
	NoiseGen.SetFractalLacunarity(Lacunarity);
}

void ANoiseGenerator::RandomiseSeed()
{
	Seed = rand();
	NoiseGen.SetSeed(Seed);
}

// Used to set global default properties of chunks, if they are not set
void ANoiseGenerator::SetUpChunk(int TerrainIndex)
{
	FChunkProperties* WorldHandle = &World[TerrainIndex];

	if (!WorldHandle->TerrainMaterial)
		WorldHandle->TerrainMaterial = DefaultMaterial;
	if (!WorldHandle->HeightCurve)
		WorldHandle->HeightCurve = DefaultHeightCurve;
}

// Called every frame
void ANoiseGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
