#pragma once
#include <glm/glm.hpp>

//Straight port of https://github.com/cabbibo/glsl-curl-noise/blob/master/curl.glsl
class CurlNoise2D
{
private:
	static glm::vec3 snoiseVec3(glm::vec3 x);

public:
	static glm::vec3 curlNoise(const glm::vec3& p);
};
