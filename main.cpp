

#include <iostream>
#include <time.h>
#include <math.h>

//Must be before libtarga, libtarga's "internal" defines override glm types, making the whole thing not compile
#include <gli/gli.hpp>
#include <gli/generate_mipmaps.hpp>

#include "./TileableVolumeNoise.h"
#include "./libtarga.h"

#include <ppl.h>
#include "NoiseKernel.h"
using namespace concurrency;

void writeTGA(const char* fileName, int width, int height, /*const*/ unsigned char* data)
{
	if (!tga_write_raw(fileName, width, height, data, TGA_TRUECOLOR_32))
	{
		printf("Failed to write image!\n");
		printf(tga_error_string(tga_get_last_error()));
	}
}

void writeDDS(const char* fileName, int width, int height, int depth, /*const*/ unsigned char* data)
{
	gli::texture3d Texture(gli::format::FORMAT_RGBA8_UNORM_PACK8, gli::extent3d(width, height, depth));
	//I'm not a 100% sure that this is an intended usage of the data() function, but it works, and by looking at the library code it should perfectly function.
	memcpy(Texture.data(), data, sizeof(unsigned char) * 4 * width * height * depth);
	gli::generate_mipmaps(Texture, gli::filter::FILTER_LINEAR);
	gli::save_dds(Texture, fileName);
}

void writeDDSMonochrome(const char* fileName, int width, int height, int depth, /*const*/ unsigned char* data)
{
	gli::texture3d Texture(gli::format::FORMAT_R8_UNORM_PACK8, gli::extent3d(width, height, depth));
	//I'm not a 100% sure that this is an intended usage of the data() function, but it works, and by looking at the library code it should perfectly function.
	memcpy(Texture.data(), data, sizeof(unsigned char) * width * height * depth);
	gli::generate_mipmaps(Texture, gli::filter::FILTER_LINEAR);
	gli::save_dds(Texture, fileName);
}

// the remap function used in the shaders as described in Gpu Pro 7. It must match when using pre packed textures
float remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
{
	return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

void CloudBaseShapeUnpackedBase(const glm::vec3& coord, float& PerlinWorleyNoise, float& worleyFBM0, float& worleyFBM1, float& worleyFBM2)
{
	// Frequence multiplicator. No boudary check etc. but fine for this small tool.
	const float frequenceMul[6] = { 2, 8, 14, 20, 26, 32 };	// special weight for perling worley

	// Perlin FBM noise
	const int octaveCount = 3;
	const float frequency = 8;
	float perlinNoise = Tileable3dNoise::PerlinNoise(coord, frequency, octaveCount);

	PerlinWorleyNoise = 0.0f;
	{
		const float cellCount = 4;
		const float worleyNoise0 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * frequenceMul[0]));
		const float worleyNoise1 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * frequenceMul[1]));
		const float worleyNoise2 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * frequenceMul[2]));
		const float worleyNoise3 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * frequenceMul[3]));
		const float worleyNoise4 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * frequenceMul[4]));
		const float worleyNoise5 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * frequenceMul[5]));	// half the frequency of texel, we should not go further (with cellCount = 32 and texture size = 64)

		// PerlinWorley noise as described p.101 of GPU Pro 7
		float worleyFBM = worleyNoise0*0.625f + worleyNoise1*0.25f + worleyNoise2*0.125f;

		PerlinWorleyNoise = remap(perlinNoise, 0.0, 1.0, worleyFBM, 1.0);
	}

	const float cellCount = 4;
	float worleyNoise0 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 1));
	float worleyNoise1 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 2));
	float worleyNoise2 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 4));
	float worleyNoise3 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 8));
	float worleyNoise4 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 16));
	//float worleyNoise5 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 32));	//cellCount=2 -> half the frequency of texel, we should not go further (with cellCount = 32 and texture size = 64)

	// Three frequency of Worley FBM noise
	worleyFBM0 = worleyNoise1*0.625f + worleyNoise2*0.25f + worleyNoise3*0.125f;
	worleyFBM1 = worleyNoise2*0.625f + worleyNoise3*0.25f + worleyNoise4*0.125f;
	//float worleyFBM2 = worleyNoise3*0.625f + worleyNoise4*0.25f + worleyNoise5*0.125f;
	worleyFBM2 = worleyNoise3*0.75f + worleyNoise4*0.25f; // cellCount=4 -> worleyNoise5 is just noise due to sampling frequency=texel frequency. So only take into account 2 frequencies for FBM
}

void GenerateCloudBaseShapeNoisePacked()
{
	// Cloud base shape (will be used to generate PerlingWorley noise in he shader)
	// Note: all channels could be combined once here to reduce memory bandwith requirements.
	unsigned int cloudBaseShapeTextureSize = 128;		// !!! If this is reduce, you hsould also reduce the number of frequency in the fmb noise  !!!
	const glm::vec3 normFact = glm::vec3(1.0f / float(cloudBaseShapeTextureSize));
	NoiseKernelMonochrome cloudBaseShape([&](unsigned int s, unsigned int t, unsigned int r, unsigned char& red)
	{
		glm::vec3 coord = glm::vec3(s, t, r) * normFact;
		float PerlinWorleyNoise, worleyFBM0, worleyFBM1, worleyFBM2;
		CloudBaseShapeUnpackedBase(coord, PerlinWorleyNoise, worleyFBM0, worleyFBM1, worleyFBM2);
	
		float value = 0.0;
		{
			// pack the channels for direct usage in shader
			float lowFreqFBM = worleyFBM0*0.625f + worleyFBM1*0.25f + worleyFBM2*0.125f;
			float baseCloud = PerlinWorleyNoise;
			value = remap(baseCloud, -(1.0f - lowFreqFBM), 1.0f, 0.0f, 1.0f);
			// Saturate
			value = std::fminf(value, 1.0f);
			value = std::fmaxf(value, 0.0f);
		}
		red = unsigned char(255.0f*value);
	});

	auto cloudBaseShapeTexelsPacked = cloudBaseShape.RunKernel(cloudBaseShapeTextureSize, cloudBaseShapeTextureSize, cloudBaseShapeTextureSize);

	{
		writeDDSMonochrome("noiseShapePacked.dds", cloudBaseShapeTextureSize, cloudBaseShapeTextureSize, cloudBaseShapeTextureSize, cloudBaseShapeTexelsPacked.get());
	}
}

void GenerateCloudBaseShapeNoiseUnpacked()
{
	// Cloud base shape (will be used to generate PerlingWorley noise in he shader)
	// Note: all channels could be combined once here to reduce memory bandwith requirements.
	unsigned int cloudBaseShapeTextureSize = 128;		// !!! If this is reduce, you hsould also reduce the number of frequency in the fmb noise  !!!
	const glm::vec3 normFact = glm::vec3(1.0f / float(cloudBaseShapeTextureSize));
	NoiseKernel cloudBaseShape([&](unsigned int s, unsigned int t, unsigned int r, unsigned char& red, unsigned char& green, unsigned char& blue, unsigned char& alpha)
	{
		glm::vec3 coord = glm::vec3(s, t, r) * normFact;
		float PerlinWorleyNoise, worleyFBM0, worleyFBM1, worleyFBM2;
		CloudBaseShapeUnpackedBase(coord, PerlinWorleyNoise, worleyFBM0, worleyFBM1, worleyFBM2);

		red = unsigned char(255.0f*PerlinWorleyNoise);
		green = unsigned char(255.0f*worleyFBM0);
		blue = unsigned char(255.0f*worleyFBM1);
		alpha = unsigned char(255.0f*worleyFBM2);	
	});

	auto cloudBaseShapeTexelsUnpacked = cloudBaseShape.RunKernel(cloudBaseShapeTextureSize, cloudBaseShapeTextureSize, cloudBaseShapeTextureSize);

	{
		//int width = cloudBaseShapeTextureSize*cloudBaseShapeTextureSize;
		//int height = cloudBaseShapeTextureSize;
		//writeTGA("noiseShape.tga", width, height, cloudBaseShapeTexelsPacked);

		writeDDS("noiseShape.dds", cloudBaseShapeTextureSize, cloudBaseShapeTextureSize, cloudBaseShapeTextureSize, cloudBaseShapeTexelsUnpacked.get());
	}
}

void CloudErosionUnpackedBase(const glm::vec3& coord, float& worleyFBM0, float& worleyFBM1, float& worleyFBM2)
{
#if 1
	// 3 octaves
	const float cellCount = 2;
	float worleyNoise0 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 1));
	float worleyNoise1 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 2));
	float worleyNoise2 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 4));
	float worleyNoise3 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 8));
	worleyFBM0 = worleyNoise0*0.625f + worleyNoise1*0.25f + worleyNoise2*0.125f;
	worleyFBM1 = worleyNoise1*0.625f + worleyNoise2*0.25f + worleyNoise3*0.125f;
	worleyFBM2 = worleyNoise2*0.75f + worleyNoise3*0.25f; // cellCount=4 -> worleyNoise4 is just noise due to sampling frequency=texel freque. So only take into account 2 frequencies for FBM
#else
	// 2 octaves
	float worleyNoise0 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 4));
	float worleyNoise1 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 7));
	float worleyNoise2 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 10));
	float worleyNoise3 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 13));
	worleyFBM0 = worleyNoise0*0.75f + worleyNoise1*0.25f;
	worleyFBM1 = worleyNoise1*0.75f + worleyNoise2*0.25f;
	worleyFBM2 = worleyNoise2*0.75f + worleyNoise3*0.25f;
#endif
}

void GenerateCloudErosionPacked()
{
	// Detail texture behing different frequency of Worley noise
	// Note: all channels could be combined once here to reduce memory bandwith requirements.
	unsigned int cloudErosionTextureSize = 32;	
	const glm::vec3 normFact = glm::vec3(1.0f / float(cloudErosionTextureSize));
	NoiseKernelMonochrome cloudErosion([&](unsigned int s, unsigned int t, unsigned int r, unsigned char& red)
	{
		glm::vec3 coord = glm::vec3(s, t, r) * normFact;

		float worleyFBM0, worleyFBM1, worleyFBM2;
		CloudErosionUnpackedBase(coord, worleyFBM0, worleyFBM1, worleyFBM2);

		float value = 0.0f;
		{
			value = worleyFBM0*0.625f + worleyFBM1*0.25f + worleyFBM2*0.125f;
		}
		red = unsigned char(255.0f * value);
	});

	auto cloudErosionTexelsPacked = cloudErosion.RunKernel(cloudErosionTextureSize, cloudErosionTextureSize, cloudErosionTextureSize);

	{		
		writeDDSMonochrome("noiseErosionPacked.dds", cloudErosionTextureSize, cloudErosionTextureSize, cloudErosionTextureSize, cloudErosionTexelsPacked.get());
	}
}

void GenerateCloudErosionUnpacked()
{
	// Detail texture behing different frequency of Worley noise
	// Note: all channels could be combined once here to reduce memory bandwith requirements.
	unsigned int cloudErosionTextureSize = 32;
	const glm::vec3 normFact = glm::vec3(1.0f / float(cloudErosionTextureSize));
	NoiseKernel cloudErosion([&](unsigned int s, unsigned int t, unsigned int r, unsigned char& red, unsigned char& green, unsigned char& blue, unsigned char& alpha)
	{
		glm::vec3 coord = glm::vec3(s, t, r) * normFact;

		float worleyFBM0, worleyFBM1, worleyFBM2;
		CloudErosionUnpackedBase(coord, worleyFBM0, worleyFBM1, worleyFBM2);

		red = unsigned char(255.0f * worleyFBM0);
		green = unsigned char(255.0f * worleyFBM1);
		blue = unsigned char(255.0f * worleyFBM2);
		alpha = unsigned char(255.0f);
	});

	auto cloudErosionTexelsUnpacked = cloudErosion.RunKernel(cloudErosionTextureSize, cloudErosionTextureSize, cloudErosionTextureSize);

	{
		//int width = cloudErosionTextureSize*cloudErosionTextureSize;
		//int height = cloudErosionTextureSize;
		//writeTGA("noiseErosion.tga", width, height, cloudErosionTexelsPacked);

		writeDDS("noiseErosion.dds", cloudErosionTextureSize, cloudErosionTextureSize, cloudErosionTextureSize, cloudErosionTexelsUnpacked.get());
	}
}

void PerlinNoiseExample()
{
	//
	// Exemple of tileable Perlin noise texture generation
	//
	unsigned int gPerlinNoiseTextureSize = 32;

	const glm::vec3 normFactPerlin = glm::vec3(1.0f / float(gPerlinNoiseTextureSize));
	NoiseKernelMonochrome perlinKernel([&](unsigned int s, unsigned int t, unsigned int r, unsigned char& red)
	{
		glm::vec3 coord = glm::vec3(s, t, r) * normFactPerlin;

		const int octaveCount = 1;
		const float frequency = 8;
		float noise = Tileable3dNoise::PerlinNoise(coord, frequency, octaveCount);
		noise *= 255.0f;

		red = unsigned char(noise);
	});

	perlinKernel.RunKernel(gPerlinNoiseTextureSize, gPerlinNoiseTextureSize, gPerlinNoiseTextureSize);
}

void WorleyNoiseExample()
{
	//
	// Exemple of tileable Worley noise texture generation
	//
	unsigned int gWorleyNoiseTextureSize = 32;
	const glm::vec3 normFactWorley = glm::vec3(1.0f / float(gWorleyNoiseTextureSize));

	NoiseKernelMonochrome perlinKernel([&](unsigned int s, unsigned int t, unsigned int r, unsigned char& red)
	{
		glm::vec3 coord = glm::vec3(s, t, r) * normFactWorley;

		const float cellCount = 3;
		float noise = 1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount);
		noise *= 255.0f;

		red = unsigned char(noise);
	});

	perlinKernel.RunKernel(gWorleyNoiseTextureSize, gWorleyNoiseTextureSize, gWorleyNoiseTextureSize);
}

int main (int argc, char *argv[])
{   
	//PerlinNoiseExample();
	//WorleyNoiseExample();

	//
	// Generate cloud shape and erosion texture similarly GPU Pro 7 chapter II-4
	//
	GenerateCloudBaseShapeNoisePacked();
	GenerateCloudErosionPacked();

    return 0;
}
