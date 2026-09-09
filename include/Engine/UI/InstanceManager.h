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
	int m_selectedModel = -1;
	std::vector<std::string> m_componentTypes;
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
