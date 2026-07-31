#include "Engine/Component.h"

#include "Engine/Entity.h"
#include "Engine/EventManager.h"
#include "Engine/Root.h"

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
