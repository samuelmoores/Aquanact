#include "TriggerSphereBedroomSecondwall.h"

#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Root.h"
#include "Engine/UI/GameGUI.h"
#include "Engine/UI/GameGUIManager.h"
#include "PlayerController.h"

void TriggerSphereBedroomSecondwall::OnEntityEnter(Entity& triggerOwner, Entity& enteringEntity)
{
	if (!enteringEntity.GetComponent<PlayerController>())
		return;

	if (GameGUI* gui = Root::Current().FrontEnd().RuntimeGUI().Runtime())
		gui->SetRuntimeWidgetVisible("interact", true,
			triggerOwner.Name() + "::" + enteringEntity.Name());
}

void TriggerSphereBedroomSecondwall::OnEntityExit(Entity& triggerOwner, Entity& leavingEntity)
{
	if (!leavingEntity.GetComponent<PlayerController>())
		return;

	if (GameGUI* gui = Root::Current().FrontEnd().RuntimeGUI().Runtime())
		gui->SetRuntimeWidgetVisible("interact", false,
			triggerOwner.Name() + "::" + leavingEntity.Name());
}
