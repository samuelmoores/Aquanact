#include "Engine/CameraManager.h"

#include "Engine/CameraController.h"
#include "Engine/EditorCameraController.h"
#include "Engine/EngineCamera.h"
#include "Engine/GameCamera.h"
#include "Engine/Input.h"

void CameraManager::startUp(EngineCamera& engineCamera, GameCamera& gameCamera)
{
	m_engineCamera = &engineCamera;
	m_gameCamera = &gameCamera;
	SetEditorMode(engineCamera);
}

void CameraManager::shutDown()
{
	m_activeController.reset();
	m_activeCamera = nullptr;
	m_engineCamera = nullptr;
	m_gameCamera = nullptr;
}

void CameraManager::Update(const Input& input)
{
	if (m_activeController)
	{
		m_activeController->Update(input);
	}
}

void CameraManager::SetController(std::unique_ptr<CameraController> controller)
{
	m_activeController = std::move(controller);
}

void CameraManager::ClearController()
{
	m_activeController.reset();
}

void CameraManager::SetEditorMode(EngineCamera& engineCamera)
{
	m_engineCamera = &engineCamera;
	m_activeCamera = &engineCamera;
	m_activeController = std::make_unique<EditorCameraController>(engineCamera);
}

void CameraManager::SetGameMode(GameCamera& gameCamera)
{
	m_gameCamera = &gameCamera;
	m_activeCamera = &gameCamera;
	m_activeController.reset();
}

Camera& CameraManager::ActiveCamera()
{
	return *m_activeCamera;
}

const Camera& CameraManager::ActiveCamera() const
{
	return *m_activeCamera;
}

void CameraManager::SetActiveCamera(Camera& camera)
{
	m_activeCamera = &camera;
}
