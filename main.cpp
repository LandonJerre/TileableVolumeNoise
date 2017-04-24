

#include <iostream>
#include <time.h>
#include <math.h>

//Must be before libtarga, libtarga's "internal" defines override glm types, making the whole thing not compile
#include <gli/gli.hpp>
#include <gli/generate_mipmaps.hpp>

#include "./TileableVolumeNoise.h"
#include "./libtarga.h"

#include <ppl.h>
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

// the remap function used in the shaders as described in Gpu Pro 7. It must match when using pre packed textures
float remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
{
	return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}

int main (int argc, char *argv[])
{   
	//
	// Exemple of tileable Perlin noise texture generation
	//
	unsigned int gPerlinNoiseTextureSize = 32;
	unsigned char* perlinNoiseTexels = (unsigned char*)malloc(gPerlinNoiseTextureSize*gPerlinNoiseTextureSize*gPerlinNoiseTextureSize * sizeof(unsigned char));



	/*
	// Generate Perlin noise source
	const glm::vec3 normFactPerlin = glm::vec3(1.0f / float(gPerlinNoiseTextureSize));
	parallel_for(int(0), int(gPerlinNoiseTextureSize), [&](int s) //for (int s=0; s<giPerlinNoiseTextureSize; s++)
	{
		for (int t = 0; t<gPerlinNoiseTextureSize; t++)
		{
			for (int r = 0; r<gPerlinNoiseTextureSize; r++)
			{
				glm::vec3 coord = glm::vec3(s, t, r) * normFactPerlin;

				const int octaveCount = 1;
				const float frequency = 8;
				float noise = Tileable3dNoise::PerlinNoise(coord, frequency, octaveCount);
				noise *= 255.0f;

				int addr = r*gPerlinNoiseTextureSize*gPerlinNoiseTextureSize + t*gPerlinNoiseTextureSize + s;
				perlinNoiseTexels[addr] = unsigned char(noise);

			}
		}
	}
	); // end parallel_for
	free(perlinNoiseTexels);

	//
	// Exemple of tileable Worley noise texture generation
	//
	unsigned int gWorleyNoiseTextureSize = 32;
	unsigned char* worleyNoiseTexels = (unsigned char*)malloc(gWorleyNoiseTextureSize*gWorleyNoiseTextureSize*gWorleyNoiseTextureSize * sizeof(unsigned char));

	const glm::vec3 normFactWorley = glm::vec3(1.0f / float(gWorleyNoiseTextureSize));
	parallel_for(int(0), int(gWorleyNoiseTextureSize), [&](int s) //for (int s = 0; s<giWorleyNoiseTextureSize; s++)
	{
		for (int t = 0; t<gWorleyNoiseTextureSize; t++)
		{
			for (int r = 0; r<gWorleyNoiseTextureSize; r++)
			{
				glm::vec3 coord = glm::vec3(s, t, r) * normFactPerlin;

				const float cellCount = 3;
				float noise = 1.0 - Tileable3dNoise::WorleyNoise(coord, cellCount);
				noise *= 255.0f;

				int addr = r*gWorleyNoiseTextureSize*gWorleyNoiseTextureSize + t*gWorleyNoiseTextureSize + s;
				worleyNoiseTexels[addr] = unsigned char(noise);
			}
		}
	}
	); // end parallel_for
	free(worleyNoiseTexels);
	*/



	//
	// Generate cloud shape and erosion texture similarly GPU Pro 7 chapter II-4
	//

	// Frequence multiplicator. No boudary check etc. but fine for this small tool.
	const float frequenceMul[6] = { 2,8,14,20,26,32 };	// special weight for perling worley

														// Cloud base shape (will be used to generate PerlingWorley noise in he shader)
														// Note: all channels could be combined once here to reduce memory bandwith requirements.
	unsigned int cloudBaseShapeTextureSize = 128;		// !!! If this is reduce, you hsould also reduce the number of frequency in the fmb noise  !!!
	unsigned char* cloudBaseShapeTexels = (unsigned char*)malloc(cloudBaseShapeTextureSize*cloudBaseShapeTextureSize*cloudBaseShapeTextureSize * sizeof(unsigned char) * 4);
	unsigned char* cloudBaseShapeTexelsPacked = (unsigned char*)malloc(cloudBaseShapeTextureSize*cloudBaseShapeTextureSize*cloudBaseShapeTextureSize * sizeof(unsigned char) * 4);
	parallel_for(int(0), int(cloudBaseShapeTextureSize), [&](int s) //for (int s = 0; s<gCloudBaseShapeTextureSize; s++)
	{
		const glm::vec3 normFact = glm::vec3(1.0f / float(cloudBaseShapeTextureSize));
		for (int t = 0; t<cloudBaseShapeTextureSize; t++)
		{
			for (int r = 0; r<cloudBaseShapeTextureSize; r++)
			{
				glm::vec3 coord = glm::vec3(s, t, r) * normFact;

				// Perlin FBM noise
				const int octaveCount = 3;
				const float frequency = 8;
				float perlinNoise = Tileable3dNoise::PerlinNoise(coord, frequency, octaveCount);

				float PerlinWorleyNoise = 0.0f;
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
				float worleyFBM0 = worleyNoise1*0.625f + worleyNoise2*0.25f + worleyNoise3*0.125f;
				float worleyFBM1 = worleyNoise2*0.625f + worleyNoise3*0.25f + worleyNoise4*0.125f;
				//float worleyFBM2 = worleyNoise3*0.625f + worleyNoise4*0.25f + worleyNoise5*0.125f;
				float worleyFBM2 = worleyNoise3*0.75f + worleyNoise4*0.25f; // cellCount=4 -> worleyNoise5 is just noise due to sampling frequency=texel frequency. So only take into account 2 frequencies for FBM

				int addr = r*cloudBaseShapeTextureSize*cloudBaseShapeTextureSize + t*cloudBaseShapeTextureSize + s;

				addr *= 4;
				cloudBaseShapeTexels[addr] = unsigned char(255.0f*PerlinWorleyNoise);
				cloudBaseShapeTexels[addr + 1] = unsigned char(255.0f*worleyFBM0);
				cloudBaseShapeTexels[addr + 2] = unsigned char(255.0f*worleyFBM1);
				cloudBaseShapeTexels[addr + 3] = unsigned char(255.0f*worleyFBM2);

				float value = 0.0;
				{
					// pack the channels for direct usage in shader
					float lowFreqFBM = worleyFBM0*0.625 + worleyFBM1*0.25 + worleyFBM2*0.125;
					float baseCloud = PerlinWorleyNoise;
					value = remap(baseCloud, -(1.0 - lowFreqFBM), 1.0, 0.0, 1.0);
					// Saturate
					value = std::fminf(value, 1.0f);
					value = std::fmaxf(value, 0.0f);
				}
				cloudBaseShapeTexelsPacked[addr] = unsigned char(255.0f*value);
				cloudBaseShapeTexelsPacked[addr + 1] = unsigned char(255.0f*value);
				cloudBaseShapeTexelsPacked[addr + 2] = unsigned char(255.0f*value);
				cloudBaseShapeTexelsPacked[addr + 3] = unsigned char(255.0f);
			}
		}
	}
	); // end parallel_for
	{
		//int width = cloudBaseShapeTextureSize*cloudBaseShapeTextureSize;
		//int height = cloudBaseShapeTextureSize;
		//writeTGA("noiseShape.tga",       width, height, cloudBaseShapeTexels);
		//writeTGA("noiseShapePacked.tga", width, height, cloudBaseShapeTexelsPacked);

		writeDDS("noiseShape.dds", cloudBaseShapeTextureSize, cloudBaseShapeTextureSize, cloudBaseShapeTextureSize, cloudBaseShapeTexels);
		writeDDS("noiseShapePacked.dds", cloudBaseShapeTextureSize, cloudBaseShapeTextureSize, cloudBaseShapeTextureSize, cloudBaseShapeTexelsPacked);
	}
	free(cloudBaseShapeTexels);
	free(cloudBaseShapeTexelsPacked);






	// Detail texture behing different frequency of Worley noise
	// Note: all channels could be combined once here to reduce memory bandwith requirements.
	unsigned int cloudErosionTextureSize = 32;
	unsigned char* cloudErosionTexels = (unsigned char*)malloc(cloudErosionTextureSize*cloudErosionTextureSize*cloudErosionTextureSize * sizeof(unsigned char) * 4);
	unsigned char* cloudErosionTexelsPacked = (unsigned char*)malloc(cloudErosionTextureSize*cloudErosionTextureSize*cloudErosionTextureSize * sizeof(unsigned char) * 4);
	parallel_for(int(0), int(cloudErosionTextureSize), [&](int s) //for (int s = 0; s<gCloudErosionTextureSize; s++)
	{
		const glm::vec3 normFact = glm::vec3(1.0f / float(cloudErosionTextureSize));
		for (int t = 0; t<cloudErosionTextureSize; t++)
		{
			for (int r = 0; r<cloudErosionTextureSize; r++)
			{
				glm::vec3 coord = glm::vec3(s, t, r) * normFact;

#if 1
				// 3 octaves
				const float cellCount = 2;
				float worleyNoise0 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 1));
				float worleyNoise1 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 2));
				float worleyNoise2 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 4));
				float worleyNoise3 = (1.0f - Tileable3dNoise::WorleyNoise(coord, cellCount * 8));
				float worleyFBM0 = worleyNoise0*0.625f + worleyNoise1*0.25f + worleyNoise2*0.125f;
				float worleyFBM1 = worleyNoise1*0.625f + worleyNoise2*0.25f + worleyNoise3*0.125f;
				float worleyFBM2 = worleyNoise2*0.75f + worleyNoise3*0.25f; // cellCount=4 -> worleyNoise4 is just noise due to sampling frequency=texel freque. So only take into account 2 frequencies for FBM
#else
				// 2 octaves
				float worleyNoise0 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 4));
				float worleyNoise1 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 7));
				float worleyNoise2 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 10));
				float worleyNoise3 = (1.0f - Tileable3dNoise::WorleyNoise(coord, 13));
				float worleyFBM0 = worleyNoise0*0.75f + worleyNoise1*0.25f;
				float worleyFBM1 = worleyNoise1*0.75f + worleyNoise2*0.25f;
				float worleyFBM2 = worleyNoise2*0.75f + worleyNoise3*0.25f;
#endif

				int addr = r*cloudErosionTextureSize*cloudErosionTextureSize + t*cloudErosionTextureSize + s;
				addr *= 4;
				cloudErosionTexels[addr] = unsigned char(255.0f*worleyFBM0);
				cloudErosionTexels[addr + 1] = unsigned char(255.0f*worleyFBM1);
				cloudErosionTexels[addr + 2] = unsigned char(255.0f*worleyFBM2);
				cloudErosionTexels[addr + 3] = unsigned char(255.0f);

				float value = 0.0;
				{
					value = worleyFBM0*0.625 + worleyFBM1*0.25 + worleyFBM2*0.125;
				}
				cloudErosionTexelsPacked[addr] = unsigned char(255.0f * value);
				cloudErosionTexelsPacked[addr + 1] = unsigned char(255.0f * value);
				cloudErosionTexelsPacked[addr + 2] = unsigned char(255.0f * value);
				cloudErosionTexelsPacked[addr + 3] = unsigned char(255.0f);
			}
		}
	}
	); // end parallel_for
	{
		//int width = cloudErosionTextureSize*cloudErosionTextureSize;
		//int height = cloudErosionTextureSize;
		//writeTGA("noiseErosion.tga",       width, height, cloudErosionTexels);
		//writeTGA("noiseErosionPacked.tga", width, height, cloudErosionTexelsPacked);
		
		writeDDS("noiseErosion.dds", cloudErosionTextureSize, cloudErosionTextureSize, cloudErosionTextureSize, cloudErosionTexels);
		writeDDS("noiseErosionPacked.dds", cloudErosionTextureSize, cloudErosionTextureSize, cloudErosionTextureSize, cloudErosionTexelsPacked);
	}
	free(cloudErosionTexels);

    return 0;
}

