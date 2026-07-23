#pragma once

#include <string>
#include <utility>
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

// Base class for all game-side gameplay objects.
//
// What a child class must implement to compile:
// - TypeName(): required because it is pure virtual (`= 0`)
// - GetBindableMembers(): required because it is pure virtual (`= 0`)
//
// Why the destructor is virtual:
// - GameObject is meant to be used polymorphically through a base pointer.
// - A virtual destructor ensures the derived destructor runs correctly when a
//   child object is deleted through a GameObject*.
//
// Why TypeName() is virtual and pure:
// - Each derived class must provide its own runtime type label.
// - The engine can call TypeName() through a base pointer and get the child
//   type without needing to know the concrete class at compile time.
//
// Why GetBindableMembers() is virtual and pure:
// - Each derived class decides which variables/functions are exposed as
//   bindable metadata.
// - The editor and future UI binding system can inspect a child class through
//   the base interface.
//
// What a new child class must do:
// - inherit from GameObject
// - call the GameObject constructor from its own constructor
// - implement TypeName()
// - implement GetBindableMembers()
// - add its own gameplay state and methods as needed
class GameObject
{
public:
	explicit GameObject(std::string name);
	virtual ~GameObject() = default;

	const std::string& Name() const { return m_name; }
	void SetName(std::string name) { m_name = std::move(name); }

	virtual const char* TypeName() const = 0;
	virtual std::vector<BindableMember> GetBindableMembers() const = 0;

private:
	std::string m_name;
};
