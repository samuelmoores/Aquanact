#pragma once

#include "Engine/Core/CameraPathData.h"

#include <cstddef>

class CameraPathCreator final {
public:
	void Clear();

	void AddPoint(const glm::vec3& position, float playerProgress);
	bool RemovePoint(std::size_t index);

	CameraPathData& Data() { return m_data; }
	const CameraPathData& Data() const { return m_data; }

	int SelectedPoint() const { return m_selectedPoint; }
	void SelectPoint(int index);

	float NextSuggestedProgress() const;

private:
	CameraPathData m_data;
	int m_selectedPoint = -1;
};
