// Fill out your copyright notice in the Description page of Project Settings.

#include "NoiseGenerator.h"
#include "ProceduralMeshComponent.h"

ANoiseGenerator::ANoiseGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	NoiseGen.SetFractalType(FastNoiseLite::FractalType_FBm);
	NoiseGen.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);

	ErosionSimulator = CreateDefaultSubobject<UErosionSimulator>(TEXT("ErosionSimulator"));
	ErosionSimulator->ChunkSize = NoiseArraySize;
	ErosionSimulator->VertexSize = VertexSize;
}

// Sets up chunks in the world
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
		}
	}
	RootComponent = World[0].TerrainMesh;
}

// Update generator and simulator seed
void ANoiseGenerator::UpdateGenerator()
{
	if (bApplyRandomSeed)
	{
		MapSeed = rand();
	}
	NoiseGen.SetSeed(MapSeed);
	ErosionSimulator->ErosionSeed = MapSeed;
}

// Creates global mask for influencing global world structure
TArray<float> ANoiseGenerator::CreateMask()
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
				DataValue = FMath::Max(abs(x - HalfSquareSide), abs(y - HalfSquareSide)) / 100.f -
					(HalfSquareSide - 100.f) / 100.f;

				// Water between map center and border mountains
			else if (FMath::Max(abs(x - HalfSquareSide), abs(y - HalfSquareSide)) >= WaterSquareBoundary)
			{
				// Linear equation from 0 to 1 in the area
				DataValue = (HalfSquareSide - 100.f) / 100.f - FMath::Max(
					abs(x - HalfSquareSide), abs(y - HalfSquareSide)) / 100.f;
				// Parabolic curve from 0 to -1 and back to 0 based on previous equation in the area
				if (MoatHeightCurve) DataValue = MoatHeightCurve->GetFloatValue(DataValue);
			}

				// Mountain in the center of the map
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

// Generates procedural mesh that is used for terrain and water
void ANoiseGenerator::GenerateTerrain(int TerrainIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("GenerateTerrain: thread started - %d"), TerrainIndex);

	const float FalloffSquareSideLength = NoiseArraySize * MapSize;
	const float NoiseArraySizeSquared = FMath::Square(NoiseArraySize);
	const float NoiseArraySizeSquaredNoBoundary = FMath::Square(NoiseArraySize - 2);

	const FChunkProperties* WorldHandle = &World[TerrainIndex];

	// Get required data from struct
	UProceduralMeshComponent* Terrain = WorldHandle->TerrainMesh;
	UProceduralMeshComponent* Water = WorldHandle->WaterMesh;
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

	// The numbers are number of times array is accessed inside loop
	Vertices.Reserve(NoiseArraySizeSquared);
	Triangles.Reserve(6 * FMath::Square(MapSize));
	UV.Reserve(NoiseArraySizeSquared);
	Normals.Init(FVector(0.f), NoiseArraySizeSquared);
	TrueVertices.Reserve(NoiseArraySizeSquaredNoBoundary);
	TrueNormals.Reserve(NoiseArraySizeSquaredNoBoundary);

	WaterVertices.Reserve(NoiseArraySizeSquaredNoBoundary);
	WaterNormals.Reserve(NoiseArraySizeSquaredNoBoundary);

	UE_LOG(LogTemp, Warning, TEXT("GenerateTerrain: ChunkOffsetX - %f, ChunkOffsetY - %f"), ChunkOffsetX,
	       ChunkOffsetY);

	/* Mesh building schematic. First triangle is TL->BL->TR, second one is TR->BL->BR.
	 * TL---TR x++
	 * |  /  |
	 * BL---BR
	 * y++;
	 */

	// First double loop allocates vertices with border
	for (int y = 0; y < NoiseArraySize; y++)
	{
		for (int x = 0; x < NoiseArraySize; x++)
		{
			float Height;

			// Noise and World are clamped from 0 to 1 by HeightCurve
			if (bApplyMask)
				Height = HeightMultiplier * TerrainHeightCurve->GetFloatValue(NoiseArray[x + y * NoiseArraySize] +
					Mask[x + FalloffMapOffset + (y + WorldHandle->ChunkNumberY * NoiseArraySize) *
						FalloffSquareSideLength]);
			else
				Height = HeightMultiplier * TerrainHeightCurve->GetFloatValue(NoiseArray[x + y * NoiseArraySize]);

			Vertices.Add(FVector(StartingPositionX + VertexSize * (x - 1), StartingPositionY + VertexSize * (y - 1),
			                     Height));
		}
	}

	if (bApplyErosion) ErosionSimulator->SimulateErosion(Vertices);

	// Second double loop calculates normal values, UVs and strips the border
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

	// Third double loop combines correct vertices into triangles. 
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
	}

	// Creates objects in main thread, cause you cannot do that elsewhere
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

	UE_LOG(LogTemp, Warning, TEXT("GenerateTerrain: thread completed - %s"), *Terrain->GetName());
}

// Called when the game starts, starts async terrain generations
void ANoiseGenerator::BeginPlay()
{
	const float WorldCenter = MapSize * MapArraySize * VertexSize / 2;
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

	PlayerController->ClientSetLocation(FVector(WorldCenter, WorldCenter, 12000.f),
	                                                          FRotator(0.f)
	);
	EnableInput(PlayerController);

	if (!TerrainHeightCurve)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeginPlay: TerrainHeightCurve not set"));
		return;
	}

	UpdateWorld();
	UpdateGenerator();

	if (bApplyMask) Mask = CreateMask();
	if (bApplyErosion) ErosionSimulator->PrecalculateIndicesAndWeights();

	for (int i = 0; i < FMath::Square(MapSize); i++)
	{
		Async(EAsyncExecution::Thread, [&, i]
		{
			GenerateTerrain(i);
		});
	}
}
