#pragma once

#include "Engine/UI/EngineGuiContext.h"

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
	void DrawPhysics(Entity& entity) const;
	bool DrawStateMachineButton() const;
	void DrawComponentControls(Component& component) const;
	void DrawAddComponent(Entity& entity, bool cutscene) const;
	bool DrawDeletePopup(const Entity& entity) const;
	bool DrawRemoveComponentPopup(const Entity& entity, const Component& component, const char* popupId) const;
	EntityComponentHeaderResult DrawComponentHeader(Component& component) const;
};
