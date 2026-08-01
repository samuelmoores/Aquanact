#include "Engine/Core/Component.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/EventManager.h"
#include "Engine/Core/Root.h"

Component::~Component()
{
	if (Root::HasCurrent())
	{
		Root::Current().Events().Unsubscribe(this);
	}
}

std::string Component::BindableEventChannel(const std::string& eventName) const
{
	const std::string entityName = m_owner ? m_owner->Name() : "<unowned>";
	return entityName + "." + Name() + "." + eventName;
}

void Component::DispatchBindableEvent(const std::string& eventName) const
{
	Root::Current().Events().Dispatch(BindableEventChannel(eventName));
}
