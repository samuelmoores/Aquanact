#include "Engine/Core/EditorCameraController.h"

#include "Engine/Core/EngineCamera.h"
#include "Engine/Core/Input.h"

EditorCameraController::EditorCameraController(EngineCamera& camera)
	: m_camera(&camera)
{
}

void EditorCameraController::Update(const Input& input)
{
	if (m_camera && input.LookActive())
	{
		m_camera->UpdateFly(input);
	}
}
