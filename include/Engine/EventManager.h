#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class Event
{
public:
	using Callback = std::function<void()>;

	explicit Event(std::string name);

	const std::string& Name() const;
	void Subscribe(void* owner, Callback callback);
	void Unsubscribe(void* owner);
	void Dispatch();

private:
	struct Listener
	{
		void* owner = nullptr;
		Callback callback;
	};

	std::string m_name;
	std::vector<Listener> m_listeners;
};

class EventManager
{
public:
	EventManager();

	Event& CreateEvent(const std::string& name);
	Event& GetEvent(const std::string& name);
	Event* FindEvent(const std::string& name);
	const Event* FindEvent(const std::string& name) const;

	void Dispatch(const std::string& name);
	void Unsubscribe(void* owner);
	void Clear();

private:
	std::unordered_map<std::string, Event> m_events;
};
