#pragma once

#include <array>
#include <cmath>

#include <glm/glm.hpp>

struct FrustumPlane
{
	glm::vec3 normal{ 0.0f };
	float distance = 0.0f;
};

class Frustum
{ 
public:
	static Frustum FromViewProjection(const glm::mat4& viewProjection)
	{
		// GLM matrices use column-major indexing, so construct the matrix rows
		// explicitly before extracting the six clip-space planes.
		const glm::vec4 row0(
			viewProjection[0][0], viewProjection[1][0],
			viewProjection[2][0], viewProjection[3][0]);
		const glm::vec4 row1(
			viewProjection[0][1], viewProjection[1][1],
			viewProjection[2][1], viewProjection[3][1]);
		const glm::vec4 row2(
			viewProjection[0][2], viewProjection[1][2],
			viewProjection[2][2], viewProjection[3][2]);
		const glm::vec4 row3(
			viewProjection[0][3], viewProjection[1][3],
			viewProjection[2][3], viewProjection[3][3]);

		Frustum frustum;
		frustum.m_planes = {
			MakePlane(row3 + row0), // Left
			MakePlane(row3 - row0), // Right
			MakePlane(row3 + row1), // Bottom
			MakePlane(row3 - row1), // Top
			MakePlane(row3 + row2), // Near
			MakePlane(row3 - row2)  // Far
		};
		return frustum;
	}

	bool IntersectsAabb(const glm::vec3& minimum, const glm::vec3& maximum) const
	{
		if (!IsFinite(minimum) || !IsFinite(maximum))
		{
			// Invalid bounds should remain visible rather than causing geometry to
			// disappear because of bad source data.
			return true;
		}

		// Canonicalize the bounds so mirrored transforms or malformed callers do
		// not accidentally invert the positive-vertex selection below.
		const glm::vec3 boundsMin = glm::min(minimum, maximum);
		const glm::vec3 boundsMax = glm::max(minimum, maximum);
		constexpr float PlaneEpsilon = 1e-4f;

		for (const FrustumPlane& plane : m_planes)
		{
			const glm::vec3 positiveVertex(
				plane.normal.x >= 0.0f ? boundsMax.x : boundsMin.x,
				plane.normal.y >= 0.0f ? boundsMax.y : boundsMin.y,
				plane.normal.z >= 0.0f ? boundsMax.z : boundsMin.z);

			if (glm::dot(plane.normal, positiveVertex) + plane.distance < -PlaneEpsilon)
			{
				return false;
			}
		}

		return true;
	}

private:
	static FrustumPlane MakePlane(const glm::vec4& coefficients)
	{
		const glm::vec3 normal(coefficients);
		const float normalLength = glm::length(normal);
		if (!std::isfinite(normalLength) || normalLength <= 1e-6f)
		{
			// A zero plane never rejects an object, keeping the test conservative
			// when supplied with a degenerate camera matrix.
			return {};
		}

		return FrustumPlane{
			normal / normalLength,
			coefficients.w / normalLength
		};
	}

	static bool IsFinite(const glm::vec3& value)
	{
		return std::isfinite(value.x) &&
			std::isfinite(value.y) &&
			std::isfinite(value.z);
	}

	std::array<FrustumPlane, 6> m_planes{};
};
