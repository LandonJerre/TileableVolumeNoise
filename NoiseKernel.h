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

	std::unique_ptr<unsigned char[]> RunKernel(unsigned int width, unsigned int height, unsigned int depth, bool _2Donly = false)
	{
		if (_2Donly)
			depth = 1;
		std::unique_ptr<unsigned char[]> resultBuffer(new unsigned char[width * height * depth * 4]);

		if (KernelFunction)
		{
			if (_2Donly)
			{
				Concurrency::parallel_for(int(0), int(width), [&](int s)
				{
					for (unsigned int t = 0; t < height; t++)
					{
						int addr = t*width + s;
						addr *= 4;
						KernelFunction(s, t, 0, resultBuffer[addr], resultBuffer[addr + 1], resultBuffer[addr + 2], resultBuffer[addr + 3]);
					}
				});
			}
			else
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

	std::unique_ptr<unsigned char[]> RunKernel(unsigned int width, unsigned int height, unsigned int depth, bool _2Donly = false)
	{
		if (_2Donly)
			depth = 1;
		std::unique_ptr<unsigned char[]> resultBuffer(new unsigned char[width * height * depth]);

		if (KernelFunction)
		{
			if (_2Donly)
			{
				Concurrency::parallel_for(int(0), int(width), [&](int s)
				{
					for (unsigned int t = 0; t < height; t++)
					{
						int addr = t*width + s;
						KernelFunction(s, t, 0, resultBuffer[addr]);
					}
				});
			}
			else
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
		}

		return resultBuffer;
	}
};