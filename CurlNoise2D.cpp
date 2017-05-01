#include "CurlNoise2D.h"
#include <glm/gtc/noise.hpp>

glm::vec3 CurlNoise2D::snoiseVec3(glm::vec3 x)
{
	float s = glm::simplex(glm::vec3(x));
	float s1 = glm::simplex(glm::vec3(x.y - 19.1, x.z + 33.4, x.x + 47.2));
	float s2 = glm::simplex(glm::vec3(x.z + 74.2, x.x - 124.5, x.y + 99.4));
	glm::vec3 c = glm::vec3(s, s1, s2);
	return c;
}

glm::vec3 CurlNoise2D::curlNoise(const glm::vec3& p)
{
	const float e = 0.1f;
	glm::vec3 dx = glm::vec3(e, 0.0, 0.0);
	glm::vec3 dy = glm::vec3(0.0, e, 0.0);
	glm::vec3 dz = glm::vec3(0.0, 0.0, e);

	glm::vec3 p_x0 = snoiseVec3(p - dx);
	glm::vec3 p_x1 = snoiseVec3(p + dx);
	glm::vec3 p_y0 = snoiseVec3(p - dy);
	glm::vec3 p_y1 = snoiseVec3(p + dy);
	glm::vec3 p_z0 = snoiseVec3(p - dz);
	glm::vec3 p_z1 = snoiseVec3(p + dz);

	float x = p_y1.z - p_y0.z - p_z1.y + p_z0.y;
	float y = p_z1.x - p_z0.x - p_x1.z + p_x0.z;
	float z = p_x1.y - p_x0.y - p_y1.x + p_y0.x;

	const float divisor = 1.0f / (2.0f * e);
	return glm::normalize(glm::vec3(x, y, z) * divisor);
}
