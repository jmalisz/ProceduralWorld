// Fill out your copyright notice in the Description page of Project Settings.


#include "NoiseGenerator.h"


#include "DropletProperties.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values for this component's properties
ANoiseGenerator::ANoiseGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	NoiseGen.SetFractalType(FastNoiseLite::FractalType_FBm);
	NoiseGen.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);

	ErosionSimulator = CreateDefaultSubobject<UErosionSimulator>(TEXT("ErosionSimulator"));
	
	// ErosionSimulator->RegisterComponent();
}

// Called when the game starts
void ANoiseGenerator::BeginPlay()
{
	const float WorldCenter = MapSize * MapArraySize * VertexSize / 2;

	GetWorld()->GetFirstPlayerController()->ClientSetLocation(FVector(WorldCenter, WorldCenter, 10000.f),
	                                                          FRotator(0.f)
	);
	UpdateWorld();

	if (bApplyRandomSeed)
	{
		MapSeed = rand();
		NoiseGen.SetSeed(MapSeed);
	}

	if (!World.GetData())
	{
		UE_LOG(LogTemp, Error, TEXT("Terrain generation: HeightCurve not set"));
		return;
	}

	WorldMap = CreateFalloffMap();

	for (int i = 0; i < FMath::Square(MapSize); i++)
	{
		AsyncTask(ENamedThreads::HighThreadPriority, [&, i]()
		{
			GenerateTerrain(i);
		});
	}
}

// Called every frame, unused
void ANoiseGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Creates perlin noise array for selected chunk based on it's offset
TArray<float> ANoiseGenerator::CreateNoiseData(float LocalOffsetX, float LocalOffsetY)
{
	// Set up generator variables
	NoiseGen.SetFractalOctaves(Octaves);
	NoiseGen.SetFractalLacunarity(Lacunarity);

	TArray<float> NoiseData;
	const float HalfNoiseArraySize = NoiseArraySize / 2;

	NoiseData.Reserve(FMath::Square(NoiseArraySize));

	for (int y = 0; y < NoiseArraySize; y++)
	{
		for (int x = 0; x < NoiseArraySize; x++)
		{
			const float SampleX = (x + LocalOffsetX - HalfNoiseArraySize) * NoiseScale + GlobalOffsetX;
			const float SampleY = (y + LocalOffsetY - HalfNoiseArraySize) * NoiseScale + GlobalOffsetY;
			const float NoiseValue = NoiseGen.GetNoise(SampleX, SampleY);

			NoiseData.Add((NoiseValue + 1) / 2);
		}
	}

	return NoiseData;
}

// Utility function exposed and used in blueprint for noise visualization
UTexture2D* ANoiseGenerator::CreateNoiseTexture(TArray<float> NoiseArray)
{
	UTexture2D* NoiseMap = UTexture2D::CreateTransient(NoiseArraySize, NoiseArraySize);
	TArray<FColor> ColorMap;

	ColorMap.Reserve(NoiseArraySize * NoiseArraySize);

	for (int y = 0; y < NoiseArraySize; y++)
	{
		for (int x = 0; x < NoiseArraySize; x++)
		{
			const int Grayscale = FMath::Lerp(0, 255, NoiseArray[x + y * NoiseArraySize]);
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

// Creates global map for influencing global world structure
TArray<float> ANoiseGenerator::CreateFalloffMap()
{
	TArray<float> MapData;
	const float SquareSideLength = NoiseArraySize * MapSize;
	const float BorderMountainSquareBoundary = SquareSideLength / 2.f - 100.f;
	const float WaterSquareBoundary = SquareSideLength / 2.f - 200.f;
	const float HalfSquareSide = SquareSideLength / 2.f;

	float DataValue = 0;

	MapData.Reserve(FMath::Square(SquareSideLength));

	for (int y = 0; y < SquareSideLength; y++)
	{
		for (int x = 0; x < SquareSideLength; x++)
		{
			DataValue = 0;

			// Mountains blocking map
			if (FMath::Max(abs(x - HalfSquareSide), abs(y - HalfSquareSide)) >= BorderMountainSquareBoundary)
				// Linear equation from 1 to 0 in the area
				DataValue = FMath::Max(abs(x - HalfSquareSide), abs(y - HalfSquareSide)) /
					100.f - (HalfSquareSide - 100.f) / 100.f;
				// Water between player map and border mountains
			else if (FMath::Max(abs(x - HalfSquareSide), abs(y - HalfSquareSide)) >= WaterSquareBoundary)
			{
				// Linear equation from 0 to 1 in the area
				DataValue = (HalfSquareSide - 100.f) / 100.f - FMath::Max(
						abs(x - HalfSquareSide), abs(y - HalfSquareSide)) /
					100.f;
				// Parabolic curve from 0 to -1 and back to 0 based on previous equation in the area
				if (SeaHeightCurve)
					DataValue = SeaHeightCurve->GetFloatValue(DataValue);
			}
				// Mountain in center of the map
			else if (FMath::Max(abs(x - HalfSquareSide), abs(y - HalfSquareSide)) <= 50.f)
			{
				// Parabolic equation from 0 to 1 and back to 0 in the area
				DataValue = 1 - FMath::Max(abs(x - HalfSquareSide), abs(y - HalfSquareSide)) / 50.f;
			}

			MapData.Add(DataValue);
		}
	}

	return MapData;
}

UTexture2D* ANoiseGenerator::CreateFalloffTexture(TArray<float> FalloffArray)
{
	const float SquareSideLength = NoiseArraySize * MapSize;

	UTexture2D* FalloffMap = UTexture2D::CreateTransient(SquareSideLength, SquareSideLength);
	TArray<FColor> ColorMap;

	ColorMap.Reserve(FMath::Square(SquareSideLength));

	for (int y = 0; y < SquareSideLength; y++)
	{
		for (int x = 0; x < SquareSideLength; x++)
		{
			const int Grayscale = FMath::Lerp(0, 255, FalloffArray[x + y * SquareSideLength]);
			ColorMap.Add(FColor(Grayscale, Grayscale, Grayscale));
		}
	}

	if (!FalloffMap)
		return nullptr;

#if WITH_EDITORONLY_DATA
	FalloffMap->MipGenSettings = TMGS_NoMipmaps;
#endif
	FalloffMap->NeverStream = true;
	FalloffMap->SRGB = 0;

	FTexture2DMipMap& Mip = FalloffMap->PlatformData->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);

	FMemory::Memcpy(Data, ColorMap.GetData(), ColorMap.GetAllocatedSize());
	Mip.BulkData.Unlock();
	FalloffMap->UpdateResource();

	return FalloffMap;
}

void ANoiseGenerator::GenerateTerrain(int TerrainIndex)
{
	const float FalloffSquareSideLength = NoiseArraySize * MapSize;
	const float NoiseArraySizeSquared = FMath::Square(NoiseArraySize);
	const float NoiseArraySizeSquaredNoBoundary = FMath::Square(NoiseArraySize - 2);

	const FChunkProperties* WorldHandle = &World[TerrainIndex];

	// Get required data from struct
	UMaterialInstance* TerrainMaterial = WorldHandle->TerrainMaterial;
	UProceduralMeshComponent* Terrain = WorldHandle->TerrainMesh;
	UMaterialInstance* WaterMaterial = WorldHandle->WaterMaterial;
	UProceduralMeshComponent* Water = WorldHandle->WaterMesh;
	const UCurveFloat* HeightCurve = WorldHandle->HeightCurve;
	const float ChunkOffsetX = WorldHandle->ChunkNumberX * MapArraySize;
	const float ChunkOffsetY = WorldHandle->ChunkNumberY * MapArraySize;
	const float FalloffMapOffset = WorldHandle->ChunkNumberX * NoiseArraySize;

	// Data for procedural mesh
	TArray<float> NoiseArray = CreateNoiseData(ChunkOffsetX, ChunkOffsetY);
	TArray<FVector> Vertices;
	TArray<FVector> WaterVertices;
	TArray<FVector2D> UV;
	TArray<FVector> Normals;
	TArray<FVector> WaterNormals;
	TArray<int32> Triangles;
	TArray<FVector> TrueVertices;
	TArray<FVector> TrueNormals;

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
	Vertices.Reserve(NoiseArraySizeSquared);
	Triangles.Reserve(6 * FMath::Square(MapSize));
	UV.Reserve(NoiseArraySizeSquared);
	Normals.Init(FVector(0.f), NoiseArraySizeSquared);
	TrueVertices.Reserve(NoiseArraySizeSquaredNoBoundary);
	TrueNormals.Reserve(NoiseArraySizeSquaredNoBoundary);

	WaterVertices.Reserve(NoiseArraySizeSquaredNoBoundary);
	WaterNormals.Reserve(NoiseArraySizeSquaredNoBoundary);

	UE_LOG(LogTemp, Warning, TEXT("Terrain generation thread calculation started: MeshName - %s"), *Terrain->GetName());
	UE_LOG(LogTemp, Warning, TEXT("Terrain generation thread: ChunkOffsetX - %f, ChunkOffsetY - %f"), ChunkOffsetX,
	       ChunkOffsetY);

	/* Mesh building schematic. First triangle is TL->BL->TR, second one is TR->BL->BR.
	 * TL---TR x++
	 * |  /  |
	 * BL---BR
	 * y++;
	 */

	// First double loop allocates Vertex coordinate. There are 2 loops because triangles need immediate access to vertices.
	for (int y = 0; y < NoiseArraySize; y++)
	{
		for (int x = 0; x < NoiseArraySize; x++)
		{
			float Height = 0.f;

			// Noise and World are clamped from 0 to 1 by HeightCurve
			if (bApplyFalloffMap)
				Height = HeightMultiplier * HeightCurve->GetFloatValue(NoiseArray[x + y * NoiseArraySize] +
					WorldMap[x + FalloffMapOffset + (y + WorldHandle->ChunkNumberY * NoiseArraySize) *
						FalloffSquareSideLength]);
			else
				Height = HeightMultiplier * HeightCurve->GetFloatValue(NoiseArray[x + y * NoiseArraySize]);

			// if (x == 0 || x == NoiseArraySize - 1 || y == 0 || y == NoiseArraySize - 1)
			// {
			// 	Height = HeightMultiplier * HeightCurve->GetFloatValue(1);
			// }
			
			Vertices.Add(FVector(StartingPositionX + VertexSize * (x - 1), StartingPositionY + VertexSize * (y - 1),
			                     Height));
		}
	}

	if (bApplyErosion)
		ErosionSimulator->SimulateErosion(Vertices);

	for (int y = 0; y < EdgeArraySize; y++)
	{
		for (int x = 0; x < EdgeArraySize; x++)
		{
			// Smooth normals calculations
			// Vertex vectors are named after their value
			const FVector VertexX = Vertices[x + y * NoiseArraySize];
			const FVector VertexXp1 = Vertices[x + 1 + y * NoiseArraySize];
			const FVector VertexYp1 = Vertices[x + (y + 1) * NoiseArraySize];
			const FVector VertexXYp1 = Vertices[x + 1 + (y + 1) * NoiseArraySize];

			const FVector CrossProduct1 = FVector::CrossProduct(VertexXp1 - VertexX, VertexYp1 - VertexX);
			const FVector CrossProduct2 = FVector::CrossProduct(VertexXp1 - VertexYp1, VertexXYp1 - VertexYp1);

			Normals[x + y * NoiseArraySize] += CrossProduct1;
			Normals[x + 1 + y * NoiseArraySize] += CrossProduct1;
			Normals[x + (y + 1) * NoiseArraySize] += CrossProduct1;
			
			Normals[x + 1 + y * NoiseArraySize] += CrossProduct2;
			Normals[x + (y + 1) * NoiseArraySize] += CrossProduct2;
			Normals[x + 1 + (y + 1) * NoiseArraySize] += CrossProduct2;

			if (x * y > 0)
			{
				WaterVertices.Add(FVector(StartingPositionX + VertexSize * (x - 1),
				                          StartingPositionY + VertexSize * (y - 1), 0.f));
				WaterNormals.Add(FVector(0.f, 0.f, 1.f));
				TrueVertices.Add(Vertices[x + y * NoiseArraySize]);
				TrueNormals.Add(Normals[x + y * NoiseArraySize]);
				UV.Add(FVector2D(UVStartingPositionX + (x - 1) * 1.0f, UVStartingPositionY + (y - 1) * 1.0f));
			}
		}
	}

	// Second double loop combines correct vertices into triangles. 
	for (int y = 0; y < MapArraySize; y++)
	{
		for (int x = 0; x < MapArraySize; x++)
		{
			// TL
			Triangles.Add(x + y * (MapArraySize + 1));
			// BL
			Triangles.Add(x + (y + 1) * (MapArraySize + 1));
			// TR
			Triangles.Add(x + 1 + y * (MapArraySize + 1));
			// TR
			Triangles.Add(x + 1 + y * (MapArraySize + 1));
			// BL
			Triangles.Add(x + (y + 1) * (MapArraySize + 1));
			// BR
			Triangles.Add(x + 1 + (y + 1) * (MapArraySize + 1));
		}
	}

	for (int i = 0; i < TrueNormals.Num(); i++)
	{
		TrueNormals[i].Normalize();
		if (!TrueNormals[i].IsNormalized())
		UE_LOG(LogTemp, Warning, TEXT("Normal is not normalized"));
	}

	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		Terrain->CreateMeshSection(0, TrueVertices, Triangles, TrueNormals, UV, TArray<FColor>(),
		                           TArray<FProcMeshTangent>(), true);
		Terrain->SetMaterial(0, TerrainMaterial);
		// ReSharper disable once CppExpressionWithoutSideEffects
		Terrain->ContainsPhysicsTriMeshData(true);

		Water->CreateMeshSection(0, WaterVertices, Triangles, WaterNormals, UV, TArray<FColor>(),
		                         TArray<FProcMeshTangent>(), false);
		Water->SetMaterial(0, WaterMaterial);
	});

	UE_LOG(LogTemp, Warning, TEXT("Terrain generation thread completed: %s"), *Terrain->GetName());
}

void ANoiseGenerator::UpdateWorld()
{
	const int ChunksNumber = FMath::Square(MapSize);

	World.Reserve(ChunksNumber);

	for (int y = 0; y < MapSize; y++)
	{
		for (int x = 0; x < MapSize; x++)
		{
			const FName TerrainObjectName = *FString::Printf(TEXT("TerrainMesh%i"), x + y * MapSize);
			World.Add(FChunkProperties());

			World[x + y * MapSize].TerrainMesh = NewObject<UProceduralMeshComponent>(
				this, UProceduralMeshComponent::StaticClass(), TerrainObjectName);
			World[x + y * MapSize].TerrainMesh->bUseAsyncCooking = true;
			World[x + y * MapSize].TerrainMesh->RegisterComponent();

			const FName WaterObjectName = *FString::Printf(TEXT("WaterMesh%i"), x + y * MapSize);
			World.Add(FChunkProperties());

			World[x + y * MapSize].WaterMesh = NewObject<UProceduralMeshComponent>(
				this, UProceduralMeshComponent::StaticClass(), WaterObjectName);
			World[x + y * MapSize].WaterMesh->RegisterComponent();

			World[x + y * MapSize].ChunkNumberX = x;
			World[x + y * MapSize].ChunkNumberY = y;
			if (!World[x + y * MapSize].HeightCurve)
				World[x + y * MapSize].HeightCurve = DefaultHeightCurve;
			if (!World[x + y * MapSize].TerrainMaterial)
				World[x + y * MapSize].TerrainMaterial = DefaultTerrainMaterial;
			if (!World[x + y * MapSize].WaterMaterial)
				World[x + y * MapSize].WaterMaterial = DefaultWaterMaterial;
		}
	}
	RootComponent = World[0].TerrainMesh;
}
