#include "Engine/Core/UIInputRouter.h"

#include <imgui.h>

UIInputRoute UIInputRouter::Resolve(
	bool gameMode,
	GameplayManager::GameState gameplayState,
	FrontEndMode frontEndMode)
{
	UIInputRoute route;
	if (frontEndMode == FrontEndMode::GameGUICreator)
	{
		route.context = Input::InputContext::GameGUIPreview;
	}
	else if (gameMode)
	{
		switch (gameplayState)
		{
		case GameplayManager::GameState::MainMenu:
			route.context = Input::InputContext::MainMenu;
			break;
		case GameplayManager::GameState::Paused:
			route.context = Input::InputContext::Paused;
			break;
		default:
			route.context = Input::InputContext::Gameplay;
			break;
		}
	}

	const ImGuiIO& imgui = ImGui::GetIO();
	route.captureMask.keyboard = imgui.WantCaptureKeyboard || imgui.WantTextInput;
	route.captureMask.mouseButtons = imgui.WantCaptureMouse || ImGui::IsAnyItemActive();
	route.captureMask.mouseMotion = route.captureMask.mouseButtons;
	const bool menuOrPause = route.context == Input::InputContext::MainMenu ||
		route.context == Input::InputContext::Paused;
	if (menuOrPause)
	{
		route.captureMask.keyboard = true;
		route.captureMask.mouseButtons = true;
		route.captureMask.mouseMotion = true;
		route.captureMask.controller = true;
	}

	const bool runtimeGameUI = gameMode || frontEndMode == FrontEndMode::GameGUICreator;
	const bool imguiOwnsMouse = imgui.WantCaptureMouse || ImGui::IsAnyItemActive();
	// Runtime menus are MyGUI-owned. ImGui diagnostics may be visible over the
	// menu, but they must not prevent the menu from receiving hover/click input.
	// In gameplay and preview, normal ImGui capture rules still apply.
	route.dispatchMouseToMyGUI = runtimeGameUI && (menuOrPause || !imguiOwnsMouse);
	return route;
}
