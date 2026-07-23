#include "Game/Enemy.h"

#include "Engine/EventManager.h"

#include <iostream>
#include <utility>

Enemy::Enemy(std::string name)
	: GameObject(std::move(name))
{
}

void Enemy::SubscribeToStart(EventManager& events)
{
	m_events = &events;
	events.GetEvent("Start").Subscribe(this, [this]()
	{
		OnStart();
	});
}

void Enemy::OnStart()
{
	std::cout << "Enemy dispatching Damage event\n";
	if (m_events)
	{
		m_events->Dispatch("Damage");
	}
}
