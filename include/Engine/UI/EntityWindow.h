#pragma once

#include "Engine/UI/EngineGuiContext.h"

#include <string>
#include <array>

class Entity;
class Component;
class EntityStateMachine;

struct EntityComponentHeaderResult
{
	bool expanded = false;
	bool removeRequested = false;
};

struct EntityWindowResult
{
	EntityStateMachine* stateMachineToDraw = nullptr;
	bool openStateMachine = false;
	bool entityDeleted = false;
};

class EntityWindow
{
public:
	EntityWindowResult Draw(const EngineGuiFrameContext& context, bool& open) const;
	void DrawTransform(Entity& entity) const;
	void DrawSelectedSubmesh(Entity& entity, int selectedSubmeshIndex) const;
	void DrawPhysics(Entity& entity) const;
	bool DrawStateMachineButton() const;
	void DrawComponentControls(Component& component) const;
	void DrawAddComponent(Entity& entity, bool cutscene, const EngineGuiFrameContext& context) const;
	void DrawTriggerSphereCodePopup(const EngineGuiFrameContext& context) const;
	bool DrawDeletePopup(const Entity& entity) const;
	bool DrawRemoveComponentPopup(const Entity& entity, const Component& component, const char* popupId) const;
	EntityComponentHeaderResult DrawComponentHeader(Component& component) const;

	mutable std::string m_pendingTriggerClassName;
	mutable std::string m_triggerCodeStatusMessage;
	mutable unsigned int m_pendingTriggerEntityId = 0;
	mutable bool m_openTriggerCodePopupRequested = false;
	mutable const void* m_materialPathMesh = nullptr;
	mutable int m_materialPathSubmesh = -1;
	mutable std::array<char, 1024> m_baseColorPathInput{};
	mutable std::array<char, 1024> m_roughnessPathInput{};
	mutable bool m_materialPathError = false;
};
