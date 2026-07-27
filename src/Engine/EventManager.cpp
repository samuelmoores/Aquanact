#include "Engine/EventManager.h"

#include <algorithm>

Event::Event(std::string name)
	: m_name(std::move(name))
{
}

const std::string& Event::Name() const
{
	return m_name;
}

void Event::Subscribe(void* owner, Callback callback)
{
	Unsubscribe(owner);
	m_listeners.push_back({ owner, std::move(callback) });
}

void Event::Unsubscribe(void* owner)
{
	const auto it = std::remove_if(m_listeners.begin(), m_listeners.end(),
		[owner](const Listener& listener)
		{
			return listener.owner == owner;
		});
	m_listeners.erase(it, m_listeners.end());
}

void Event::Dispatch()
{
	for (const auto& listener : m_listeners)
	{
		if (listener.callback)
		{
			listener.callback();
		}
	}
}

EventManager::EventManager()
{
	CreateEvent("Damage");
}

Event& EventManager::CreateEvent(const std::string& name)
{
	auto [it, inserted] = m_events.try_emplace(name, name);
	(void)inserted;
	return it->second;
}

Event& EventManager::GetEvent(const std::string& name)
{
	return CreateEvent(name);
}

Event* EventManager::FindEvent(const std::string& name)
{
	auto it = m_events.find(name);
	return it != m_events.end() ? &it->second : nullptr;
}

const Event* EventManager::FindEvent(const std::string& name) const
{
	auto it = m_events.find(name);
	return it != m_events.end() ? &it->second : nullptr;
}

void EventManager::Dispatch(const std::string& name)
{
	if (Event* event = FindEvent(name))
	{
		event->Dispatch();
	}
}

void EventManager::Unsubscribe(void* owner)
{
	for (auto& [name, event] : m_events)
	{
		(void)name;
		event.Unsubscribe(owner);
	}
}

void EventManager::Clear()
{
	m_events.clear();
	CreateEvent("Damage");
}
