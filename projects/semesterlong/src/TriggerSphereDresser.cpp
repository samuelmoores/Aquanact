#include "TriggerSphereDresser.h"

#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Root.h"
#include "Engine/UI/GameGUI.h"
#include "Engine/UI/GameGUIManager.h"
#include "PlayerController.h"

#include <string>

namespace
{
	void SetInteractImageVisible(bool visible, const std::string& source)
	{
		if (GameGUI* gui = Root::Current().FrontEnd().RuntimeGUI().Runtime())
			gui->SetRuntimeWidgetVisible("interact", visible, source);
	}
}

void TriggerSphereDresser::OnEntityEnter(Entity& triggerOwner, Entity& enteringEntity)
{
	if (!enteringEntity.GetComponent<PlayerController>())
		return;

	SetInteractImageVisible(true, triggerOwner.Name() + "::" + enteringEntity.Name());
}

void TriggerSphereDresser::OnEntityExit(Entity& triggerOwner, Entity& leavingEntity)
{
	if (!leavingEntity.GetComponent<PlayerController>())
		return;

	SetInteractImageVisible(false, triggerOwner.Name() + "::" + leavingEntity.Name());
}
