#pragma once

#include "Engine/Core/CameraPathData.h"

#include <cstddef>

class CameraPathCreator final {
public:
	enum class SelectedPart { Camera, Trigger };

	void Clear();

	void AddPoint(const glm::vec3& position);
	bool RemovePoint(std::size_t index);

	CameraPathData& Data() { return m_data; }
	const CameraPathData& Data() const { return m_data; }

	int SelectedPoint() const { return m_selectedPoint; }
	void SelectPoint(int index);
	void SelectCamera() { m_selectedPart = SelectedPart::Camera; }
	void SelectTrigger() { m_selectedPart = SelectedPart::Trigger; }
	bool TriggerSelected() const { return m_selectedPart == SelectedPart::Trigger; }
	void TranslateAll(const glm::vec3& delta);


private:
	CameraPathData m_data;
	int m_selectedPoint = -1;
	SelectedPart m_selectedPart = SelectedPart::Camera;
};
