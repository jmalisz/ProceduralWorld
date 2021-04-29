// 

#include "Visualization.h"

// Function exposed and used in blueprint for noise visualization
UTexture2D* UVisualization::CreateNoiseTexture(TArray<float> NoiseArray, float NoiseArraySize)
{
	UTexture2D* NoiseMap = UTexture2D::CreateTransient(NoiseArraySize, NoiseArraySize);
	TArray<FColor> ColorMap;

	ColorMap.Reserve(NoiseArraySize * NoiseArraySize);

	// Colors the texture in greyscale
	for (int y = 0; y < NoiseArraySize; y++)
	{
		for (int x = 0; x < NoiseArraySize; x++)
		{
			const int Grayscale = FMath::Lerp(0, 255, NoiseArray[x + y * NoiseArraySize]);
			ColorMap.Add(FColor(Grayscale, Grayscale, Grayscale));
		}
	}

	if (!NoiseMap) return nullptr;

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

// Function exposed and used in blueprint for mask visualization
UTexture2D* UVisualization::CreateMaskTexture(TArray<float> MaskArray, float MapSideLength)
{
	UTexture2D* FalloffMap = UTexture2D::CreateTransient(MapSideLength, MapSideLength);
	TArray<FColor> ColorMap;

	ColorMap.Reserve(FMath::Square(MapSideLength));

	// Colors the texture in greyscale
	for (int y = 0; y < MapSideLength; y++)
	{
		for (int x = 0; x < MapSideLength; x++)
		{
			const int Grayscale = FMath::Lerp(0, 255, MaskArray[x + y * MapSideLength]);
			ColorMap.Add(FColor(Grayscale, Grayscale, Grayscale));
		}
	}

	if (!FalloffMap) return nullptr;

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
