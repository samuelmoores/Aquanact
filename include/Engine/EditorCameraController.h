#pragma once

#include "Engine/CameraController.h"

class EngineCamera;
class Input;

class EditorCameraController final : public CameraController {
public:
	explicit EditorCameraController(EngineCamera& camera);

	void Update(const Input& input) override;

private:
	EngineCamera* m_camera = nullptr;
};
