#pragma once

#include <string>
#include <vector>

struct BindableMember
{
	enum class Kind
	{
		Variable,
		Function,
	};

	std::string name;
	std::string displayName;
	std::string typeName;
	Kind kind = Kind::Variable;
};

struct BindableEvent
{
	std::string name;
	std::string displayName;
};

class Entity;

class Component {
public:
	virtual ~Component();

	virtual const char* Name() const = 0;
	virtual int ExecutionOrder() const { return 0; }
	virtual void startUp(Entity&) {}
	virtual void Update(Entity&, float) {}
	virtual void FirstFrame(Entity&) {}
	virtual std::vector<BindableMember> GetBindableMembers() const { return {}; }
	virtual bool TryGetBindableValue(const std::string&, float&) const { return false; }
	virtual std::vector<BindableEvent> GetBindableEvents() const { return {}; }
	virtual std::string GetBindableEventText(const std::string&) const { return {}; }

	void SetOwner(Entity* owner) { m_owner = owner; }
	Entity* Owner() const { return m_owner; }
	std::string BindableEventChannel(const std::string& eventName) const;

protected:
	void DispatchBindableEvent(const std::string& eventName) const;

private:
	Entity* m_owner = nullptr;
};

