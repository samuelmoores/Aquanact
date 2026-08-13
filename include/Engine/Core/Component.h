#pragma once

#include <string>
#include <type_traits>
#include <vector>

// Convert a bindable's C++ type into the small set of value types understood by
// editor binding controls. This keeps UI metadata consistent with the member or
// getter instead of requiring each component to repeat its type as a string.
template <typename T>
constexpr const char* AquaBindableTypeName()
{
	// A getter may return a const reference, but the editor only needs its value type.
	using ValueType = std::remove_cvref_t<T>;

	// Check bool before other integral types because C++ treats bool as integral.
	if constexpr (std::is_same_v<ValueType, bool>)
	{
		return "bool";
	}
	else if constexpr (std::is_integral_v<ValueType>)
	{
		return "int";
	}
	// Preserve double metadata before handling the remaining floating-point types.
	else if constexpr (std::is_same_v<ValueType, double>)
	{
		return "double";
	}
	else if constexpr (std::is_floating_point_v<ValueType>)
	{
		return "float";
	}
	else
	{
		return "unsupported";
	}
}

struct BindableMember
{
	// The editor uses this metadata to decide how to present a value source.
	// `name` is the lookup key, `displayName` is what the UI shows, and
	// `typeName` tells downstream systems what kind of value this binding carries.
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
	// Events are one-shot signals, not sampled state.
	// The editor uses these to populate event-binding lists, and the runtime
	// uses them to subscribe to "something happened" notifications.
	std::string name;
	std::string displayName;
};

// Macro helpers used by component-specific binding lists.
// They keep the component header compact while still generating the runtime
// metadata and lookup logic the editor needs.
#define AQUA_BIND_VALUE(memberName) \
	BindableMember{ #memberName, #memberName, AquaBindableTypeName<decltype(this->memberName)>(), BindableMember::Kind::Variable },

#define AQUA_BIND_FUNCTION(functionName) \
	BindableMember{ #functionName, #functionName, AquaBindableTypeName<decltype(this->functionName())>(), BindableMember::Kind::Function },

#define AQUA_BIND_ACCESSOR_VALUE(bindName) \
	if (memberName == #bindName) \
	{ \
		value = static_cast<float>(this->bindName); \
		matched = true; \
		return true; \
	}

#define AQUA_BIND_ACCESSOR_FUNCTION(bindName) \
	if (memberName == #bindName) \
	{ \
		value = static_cast<float>(this->bindName()); \
		matched = true; \
		return true; \
	}

// Bindable events are one-shot notifications.
// Use them for transitions or actions such as "HealthChanged", "Opened", or
// "DamageTaken", where the UI or gameplay code should react to the moment the
// change occurs.
#define AQUA_EVENT(name, textExpr) \
	BindableEvent{ #name, #name },

// Internal helper for event text. The UI can ask for a string to show when
// the event is active or when the event binding needs a caption.
#define AQUA_EVENT_TEXT(name, textExpr) \
	if (eventName == #name) \
	{ \
		return textExpr; \
	}

// Define one list macro per component and pass it here.
// The list macro must accept two callbacks:
// - one for stored values
// - one for getter functions
// The macro expands to both metadata and lookup code so the component only
// has to describe the bindings once.
// Example:
//   #define PLAYER_HEALTH_BINDABLES(BIND_VALUE, BIND_FUNCTION) \
//       BIND_FUNCTION(Health) \
//       BIND_VALUE(m_health)
//   AQUA_DECLARE_BINDABLES(PLAYER_HEALTH_BINDABLES)
#define AQUA_DECLARE_BINDABLES(bindableListMacro) \
	std::vector<BindableMember> GetBindableMembers() const override \
	{ \
		return { bindableListMacro(AQUA_BIND_VALUE, AQUA_BIND_FUNCTION) }; \
	} \
	bool TryGetBindableValue(const std::string& memberName, float& value) const override \
	{ \
		bool matched = false; \
		bindableListMacro(AQUA_BIND_ACCESSOR_VALUE, AQUA_BIND_ACCESSOR_FUNCTION) \
		return matched; \
	}

// Begin/end pair for one-shot bindable events.
// Put AQUA_EVENT lines between these wrappers.
// The text callback is kept separate so the editor can present the event list
// and the runtime can ask for a label only when it needs one.
#define AQUA_EVENTS_BEGIN \
	std::vector<BindableEvent> GetBindableEvents() const override \
	{ \
		return {

#define AQUA_EVENTS_TEXT_END \
		}; \
	} \
	std::string GetBindableEventText(const std::string& eventName) const override \
	{

#define AQUA_EVENTS_END \
		return {}; \
	}

class Entity;

class Component {
public:
	virtual ~Component();

	// The base component contract is intentionally small:
	// - Name() identifies the type for the factory and the editor
	// - lifecycle methods are the runtime entry points
	// - bindable metadata exposes editor-visible state
	virtual const char* Name() const = 0;
	virtual int ExecutionOrder() const { return 0; }
	virtual void startUp(Entity&) {}
	virtual void Update(Entity&, float) {}
	virtual void FirstFrame(Entity&) {}
	// Components may override these hooks to react when their owning entity
	// enters or exits a TriggerSphere.
	virtual void OnTriggerEnter(Entity&) {}
	virtual void OnTriggerExit(Entity&) {}
	virtual std::vector<BindableMember> GetBindableMembers() const { return {}; }
	virtual bool TryGetBindableValue(const std::string&, float&) const { return false; }
	virtual std::vector<BindableEvent> GetBindableEvents() const { return {}; }
	virtual std::string GetBindableEventText(const std::string&) const { return {}; }

	void SetOwner(Entity* owner) { m_owner = owner; }
	Entity* Owner() const { return m_owner; }

	// Channel names are kept human-readable so debugging subscriptions is easy.
	// They are derived from the owning entity name, component type, and member
	// or event name, which is good enough for the current editor/runtime flow.
	std::string BindableEventChannel(const std::string& eventName) const;
	std::string BindableValueChannel(const std::string& memberName) const;

protected:
	void DispatchBindableEvent(const std::string& eventName) const;
	void DispatchBindableValueChanged(const std::string& memberName) const;

private:
	Entity* m_owner = nullptr;
};

