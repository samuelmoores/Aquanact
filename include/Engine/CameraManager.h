#pragma once

#include <memory>

#include "Engine/CameraController.h"

class Input;
class EngineCamera;
class GameCamera;
class Camera;
class Window;

class CameraManager {
public:
	void startUp(EngineCamera& engineCamera, GameCamera& gameCamera);
	void shutDown();
	void Update(const Input& input);
	void SetEditorMode(EngineCamera& engineCamera);
	void SetGameMode(GameCamera& gameCamera);
	void SetController(std::unique_ptr<CameraController> controller);
	void ClearController();

	Camera& ActiveCamera();
	const Camera& ActiveCamera() const;
	void SetActiveCamera(Camera& camera);

private:
	std::unique_ptr<CameraController> m_activeController;
	Camera* m_activeCamera = nullptr;
	EngineCamera* m_engineCamera = nullptr;
	GameCamera* m_gameCamera = nullptr;
};
