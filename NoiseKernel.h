#pragma once
#include <functional>
#include <memory>
#include <future>

class NoiseKernel
{
private:
	std::function<void(unsigned int, unsigned int, unsigned int, unsigned char&, unsigned char&, unsigned char&, unsigned char&)> KernelFunction;
public:
	NoiseKernel(const std::function<void(unsigned int, unsigned int, unsigned int, unsigned char&, unsigned char&, unsigned char&, unsigned char&)>& kernelFunction) : KernelFunction(kernelFunction)
	{		
	}

	std::unique_ptr<unsigned char[]> RunKernel(unsigned int width, unsigned int height, unsigned int depth)
	{
		std::unique_ptr<unsigned char[]> resultBuffer(new unsigned char[width * height * depth * 4]);

		if (KernelFunction)
		{
			Concurrency::parallel_for(int(0), int(width), [&](int s)
			{
				for (unsigned int t = 0; t < height; t++)
				{
					for (unsigned int r = 0; r < depth; r++)
					{
						int addr = r*width*height + t*depth + s;
						addr *= 4;
						KernelFunction(s, t, r, resultBuffer[addr], resultBuffer[addr + 1], resultBuffer[addr + 2], resultBuffer[addr + 3]);
					}
				}
			});
		}

		return resultBuffer;
	}
};

class NoiseKernelMonochrome
{
private:
	std::function<void(unsigned int, unsigned int, unsigned int, unsigned char&)> KernelFunction;
public:
	NoiseKernelMonochrome(const std::function<void(unsigned int, unsigned int, unsigned int, unsigned char&)>& kernelFunction) : KernelFunction(kernelFunction)
	{
	}

	std::unique_ptr<unsigned char[]> RunKernel(unsigned int width, unsigned int height, unsigned int depth)
	{
		std::unique_ptr<unsigned char[]> resultBuffer(new unsigned char[width * height * depth]);

		if (KernelFunction)
		{
			Concurrency::parallel_for(int(0), int(width), [&](int s)
			{
				for (unsigned int t = 0; t < height; t++)
				{
					for (unsigned int r = 0; r < depth; r++)
					{
						int addr = r*width*height + t*depth + s;
						KernelFunction(s, t, r, resultBuffer[addr]);
					}
				}
			});
		}

		return resultBuffer;
	}
};