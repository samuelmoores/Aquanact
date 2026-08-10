#include "Engine/Core/Component.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EventManager.h"
#include "Engine/Core/Root.h"

namespace
{
	// Keep channel construction in one place so the format stays consistent
	// across event dispatch and any future subscription helpers.
	std::string MakeBindableChannel(const Component& component, const std::string& suffix, bool includeValuePrefix)
	{
		const std::string entityName = component.Owner() ? component.Owner()->Name() : "<unowned>";
		std::string channel = entityName + "." + component.Name() + ".";
		if (includeValuePrefix)
		{
			channel += "value.";
		}
		channel += suffix;
		return channel;
	}
}

Component::~Component()
{
	// Components can subscribe to engine events, so tear those subscriptions
	// down automatically when the component is destroyed.
	if (Root::HasCurrent())
	{
		Root::Current().Events().Unsubscribe(this);
	}
}

std::string Component::BindableEventChannel(const std::string& eventName) const
{
	return MakeBindableChannel(*this, eventName, false);
}

std::string Component::BindableValueChannel(const std::string& memberName) const
{
	return MakeBindableChannel(*this, memberName, true);
}

void Component::DispatchBindableEvent(const std::string& eventName) const
{
	// Dispatch through the common channel so editor bindings and runtime
	// subscribers use the same event name.
	Root::Current().Events().Dispatch(BindableEventChannel(eventName));
}

void Component::DispatchBindableValueChanged(const std::string& memberName) const
{
	// Value changes are treated as notifications that a sampled value should
	// be refreshed by the UI or any runtime listeners.
	Root::Current().Events().Dispatch(BindableValueChannel(memberName));
}
