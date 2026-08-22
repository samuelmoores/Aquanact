#pragma once

#include <glm/glm.hpp>

class CameraPathCreator;

class CameraWindow
{
public:
	void Draw(CameraPathCreator& cameraPath, bool& open, bool& showCameraPath);

private:
	glm::vec3 m_curveTranslation{ 0.0f };
};
