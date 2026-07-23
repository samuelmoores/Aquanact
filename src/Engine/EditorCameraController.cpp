#include "Engine/EditorCameraController.h"

#include "Engine/EngineCamera.h"
#include "Engine/Input.h"

EditorCameraController::EditorCameraController(EngineCamera& camera)
	: m_camera(&camera)
{
}

void EditorCameraController::Update(const Input& input)
{
	if (m_camera)
	{
		m_camera->UpdateFly(input);
	}
}
