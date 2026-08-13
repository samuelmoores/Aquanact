#include "Engine/UI/CameraPathCreator.h"

#include <algorithm>
#include <cmath>

void CameraPathCreator::Clear()
{
	m_data.points.clear();
	m_selectedPoint = -1;
}

void CameraPathCreator::AddPoint(
	const glm::vec3& position,
	float playerProgress)
{
	if (!std::isfinite(position.x) ||
		!std::isfinite(position.y) ||
		!std::isfinite(position.z) ||
		!std::isfinite(playerProgress))
	{
		return;
	}

	const float minimumProgress = m_data.points.empty()
		? 0.0f
		: m_data.points.back().playerProgress + 0.001f;

	CameraPathPoint point;
	point.position = position;
	point.playerProgress = std::max(playerProgress, minimumProgress);
	m_data.points.push_back(point);
	m_selectedPoint = static_cast<int>(m_data.points.size()) - 1;
}

bool CameraPathCreator::RemovePoint(std::size_t index)
{
	// Reject invalid indices before changing either the path or selection.
	if (index >= m_data.points.size())
	{
		return false;
	}

	// std::vector::erase expects an iterator, so convert the index to one.
	m_data.points.erase(m_data.points.begin() + index);

	if (m_data.points.empty())
	{
		// No points remain, so no point can be selected.
		m_selectedPoint = -1;
	}
	else if (m_selectedPoint == static_cast<int>(index))
	{
		// The selected point was deleted. Select the next point, or the previous
		// final point when the deleted point was at the end.
		m_selectedPoint = static_cast<int>(std::min(index, m_data.points.size() - 1));
	}
	else if (m_selectedPoint > static_cast<int>(index))
	{
		// Removing an earlier point shifts the selected point one position left.
		--m_selectedPoint;
	}

	return true;
}

void CameraPathCreator::SelectPoint(int index)
{
	if (index < 0 || index >= static_cast<int>(m_data.points.size()))
	{
		m_selectedPoint = -1;
		return;
	}

	m_selectedPoint = index;
}

float CameraPathCreator::NextSuggestedProgress() const
{
	return m_data.points.empty()
		? 0.0f
		: m_data.points.back().playerProgress + 10.0f;
}
