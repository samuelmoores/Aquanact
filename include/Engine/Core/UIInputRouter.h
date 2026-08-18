#pragma once

#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/Core/GameplayManager.h"

struct UIInputRoute
{
	Input::InputContext context = Input::InputContext::Editor;
	InputCaptureMask captureMask;
	bool dispatchMouseToMyGUI = false;
};

class UIInputRouter
{
public:
	static UIInputRoute Resolve(bool gameMode, GameplayManager::GameState gameplayState, FrontEndMode frontEndMode);
};
