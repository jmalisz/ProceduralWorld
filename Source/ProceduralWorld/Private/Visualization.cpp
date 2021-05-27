// Fill out your copyright notice in the Description page of Project Settings.

#include "Visualization.h"

// Function exposed and used in blueprint for texture visualization
UTexture2D* UVisualization::CreateTexture(TArray<float> ValueArray, float ValueArrayRowNum)
{
	UTexture2D* GeneratedTexture = UTexture2D::CreateTransient(ValueArrayRowNum, ValueArrayRowNum);
	TArray<FColor> ColorMap;

	ColorMap.Reserve(ValueArrayRowNum * ValueArrayRowNum);

	// Colors the texture in grayscale
	for (int y = 0; y < ValueArrayRowNum; y++)
	{
		for (int x = 0; x < ValueArrayRowNum; x++)
		{
			const int Grayscale = FMath::Lerp(0, 255, ValueArray[x + y * ValueArrayRowNum]);
			ColorMap.Add(FColor(Grayscale, Grayscale, Grayscale));
		}
	}

	if (!GeneratedTexture) return nullptr;

#if WITH_EDITORONLY_DATA
	GeneratedTexture->MipGenSettings = TMGS_NoMipmaps;
#endif
	GeneratedTexture->NeverStream = true;
	GeneratedTexture->SRGB = 0;

	FTexture2DMipMap& Mip = GeneratedTexture->PlatformData->Mips[0];

	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, ColorMap.GetData(), ColorMap.GetAllocatedSize());
	Mip.BulkData.Unlock();
	
	GeneratedTexture->UpdateResource();

	return GeneratedTexture;
}
