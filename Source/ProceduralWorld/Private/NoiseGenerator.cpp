// Fill out your copyright notice in the Description page of Project Settings.


#include "NoiseGenerator.h"

#include "ProceduralMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values for this component's properties
ANoiseGenerator::ANoiseGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	NoiseGen.SetFractalType(FastNoiseLite::FractalType_FBm);
	NoiseGen.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
}

// Called when the game starts
void ANoiseGenerator::BeginPlay()
{
	GetWorld()->GetFirstPlayerController()->ClientSetLocation(FVector(MapSizeX * MapArrayWidth * VertexSize / 2,
                                                                      MapSizeY * MapArrayHeight * VertexSize / 2,
                                                                      10000.f), FRotator(0.f));
	UpdateWorld();

	if (bApplyRandomSeed)
	{
		Seed = rand();
		NoiseGen.SetSeed(Seed);
	}

	if (!World.GetData())
	{
		UE_LOG(LogTemp, Error, TEXT("Terrain generation: HeightCurve not set"));
		return;
	}

	for (int i = 0; i < MapSizeX * MapSizeY; i++)
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

TArray<float> ANoiseGenerator::CreateNoiseData(float LocalOffsetX, float LocalOffsetY)
{
	// Set up generator variables
	NoiseGen.SetFractalOctaves(Octaves);
	NoiseGen.SetFractalLacunarity(Lacunarity);

	TArray<float> NoiseData;
	const float HalfNoiseArrayWidth = NoiseArrayWidth / 2;
	const float HalfNoiseArrayHeight = NoiseArrayHeight / 2;

	NoiseData.Reserve(NoiseArrayWidth * NoiseArrayHeight);

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
	UMaterialInstance* TerrainMaterial = WorldHandle->TerrainMaterial;
	UProceduralMeshComponent* Terrain = WorldHandle->TerrainMesh;
	UMaterialInstance* WaterMaterial = WorldHandle->WaterMaterial;
	UProceduralMeshComponent* Water = WorldHandle->WaterMesh;
	const UCurveFloat* HeightCurve = WorldHandle->HeightCurve;
	const float ChunkOffsetX = WorldHandle->ChunkNumberX * MapArrayWidth;
	const float ChunkOffsetY = WorldHandle->ChunkNumberY * MapArrayHeight;

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
	Vertices.Reserve(NoiseArrayWidth * NoiseArrayHeight);
	Triangles.Reserve(6 * MapSizeX * MapSizeX);
	UV.Reserve(NoiseArrayWidth * NoiseArrayHeight);
	Normals.Init(FVector(0.f), NoiseArrayWidth * NoiseArrayHeight);
	TrueVertices.Reserve((NoiseArrayWidth - 2) * (NoiseArrayHeight - 2));
	TrueNormals.Reserve((NoiseArrayWidth - 2) * (NoiseArrayHeight - 2));
	
	WaterVertices.Reserve((NoiseArrayWidth - 2) * (NoiseArrayHeight - 2));
	WaterNormals.Reserve((NoiseArrayWidth - 2) * (NoiseArrayHeight - 2));

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
			Vertices.Add(FVector(StartingPositionX + VertexSize * (x - 1), StartingPositionY + VertexSize * (y - 1),
			                     Height));
		}
	}

	for (int y = 0; y < EdgeArrayHeight; y++)
	{
		for (int x = 0; x < EdgeArrayWidth; x++)
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

			if (x * y > 0)
			{
				WaterVertices.Add(FVector(StartingPositionX + VertexSize * (x - 1), StartingPositionY + VertexSize * (y - 1), 0.f));
				WaterNormals.Add(FVector(0.f, 0.f, 1.f));
				TrueVertices.Add(Vertices[x + y * NoiseArrayHeight]);
				TrueNormals.Add(Normals[x + y * NoiseArrayHeight]);
				UV.Add(FVector2D(UVStartingPositionX + (x - 1) * 1.0f, UVStartingPositionY + (y - 1) * 1.0f));
			}
		}
	}

	// Second double loop combines correct vertices into triangles. 
	for (int y = 0; y < MapArrayHeight; y++)
	{
		for (int x = 0; x < MapArrayWidth; x++)
		{
			// TL
			Triangles.Add(x + y * (MapArrayHeight + 1));
			// BL
			Triangles.Add(x + (y + 1) * (MapArrayHeight + 1));
			// TR
			Triangles.Add(x + 1 + y * (MapArrayHeight + 1));
			// TR
			Triangles.Add(x + 1 + y * (MapArrayHeight + 1));
			// BL
			Triangles.Add(x + (y + 1) * (MapArrayHeight + 1));
			// BR
			Triangles.Add(x + 1 + (y + 1) * (MapArrayHeight + 1));
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
	const int ChunksNumber = MapSizeX * MapSizeY;

	World.Reserve(ChunksNumber);

	for (int y = 0; y < MapSizeY; y++)
	{
		for (int x = 0; x < MapSizeX; x++)
		{
			const FName TerrainObjectName = *FString::Printf(TEXT("TerrainMesh%i"), x + y * MapSizeY);
			World.Add(FChunkProperties());
			
			World[x + y * MapSizeY].TerrainMesh = NewObject<UProceduralMeshComponent>(
				this, UProceduralMeshComponent::StaticClass(), TerrainObjectName);
			World[x + y * MapSizeY].TerrainMesh->bUseAsyncCooking = true;
			World[x + y * MapSizeY].TerrainMesh->RegisterComponent();

			const FName WaterObjectName = *FString::Printf(TEXT("WaterMesh%i"), x + y * MapSizeY);
			World.Add(FChunkProperties());
			
			World[x + y * MapSizeY].WaterMesh = NewObject<UProceduralMeshComponent>(
                this, UProceduralMeshComponent::StaticClass(), WaterObjectName);
			World[x + y * MapSizeY].WaterMesh->RegisterComponent();
			
			World[x + y * MapSizeY].ChunkNumberX = x;
			World[x + y * MapSizeY].ChunkNumberY = y;
			if(!World[x + y * MapSizeY].HeightCurve)
				World[x + y * MapSizeY].HeightCurve = DefaultHeightCurve;
			if(!World[x + y * MapSizeY].TerrainMaterial)
				World[x + y * MapSizeY].TerrainMaterial = DefaultTerrainMaterial;
			if(!World[x + y * MapSizeY].WaterMaterial)
				World[x + y * MapSizeY].WaterMaterial = DefaultWaterMaterial;
		}
	}
	RootComponent = World[0].TerrainMesh;
}


