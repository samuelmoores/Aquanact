#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Engine/Core/EntityStateMachine.h"
#include "Engine/UI/EntityWindow.h"

class SceneManager;
class Entity;
struct InstanceDefinition;

struct InstanceManagerResult
{
	EntityStateMachine* stateMachineToDraw = nullptr;
	bool openStateMachine = false;
	bool closeStateMachine = false;
};

// Editor-side factory for creating configured runtime entity instances.
class InstanceManager final
{
public:
	~InstanceManager();
	InstanceManagerResult Draw(SceneManager& scenes, bool& open);
	void SyncStateMachineConfiguration();
	EntityStateMachine* StateMachinePreview() const;

private:
	char m_entityName[128] = "Enemy";
	char m_statusMessage[256] = {};
	std::vector<std::filesystem::path> m_models;
	std::vector<std::string> m_modelLabels;
	bool m_modelsLoaded = false;
	bool m_wasOpen = false;
	int m_selectedModel = -1;
	bool m_triggerVolume = false;
	float m_triggerRadius = 90.0f;
	glm::vec3 m_triggerPositionOffset{0.0f};
	std::string m_attachToEntityName;
	std::vector<std::string> m_componentTypes;
	bool m_blocksCollision = true;
	bool m_ignoreCameraCollision = false;
	bool m_blocksCameraView = true;
	bool m_showPhysicsBoundingBox = false;
	bool m_rigidBody = false;
	int m_physicsColliderShape = 0;
	std::string m_editingDefinitionName;
	EntityWindow m_entityWindow;
	std::unique_ptr<Entity> m_componentPreview;
	std::unique_ptr<Entity> m_stateMachinePreview;
	std::string m_stateMachinePreviewName;

	void RefreshModels();
	void SyncComponentPreview();
	bool OpenStateMachinePreview(const InstanceDefinition& definition);
	void DrawComponentPreview();
};
