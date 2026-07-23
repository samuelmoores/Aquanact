#pragma once

#include "Engine/Entity.h"

class EventManager;

class Enemy final : public Entity
{
public:
	explicit Enemy(std::string name = "Enemy");

	const char* TypeName() const override { return "Enemy"; }
	std::vector<BindableMember> GetBindableMembers() const override { return {}; }

	void SubscribeToStart(EventManager& events);
	void OnStart();

private:
	EventManager* m_events = nullptr;
};
