#include "Engine/Core/UIInputRouter.h"

#include <imgui.h>

#include <cassert>

int main()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	const UIInputRoute editor = UIInputRouter::Resolve(false, GameplayManager::GameState::MainMenu, FrontEndMode::EngineEditor);
	assert(editor.context == Input::InputContext::Editor);
	assert(!editor.dispatchMouseToMyGUI);

	const UIInputRoute menu = UIInputRouter::Resolve(true, GameplayManager::GameState::MainMenu, FrontEndMode::EngineEditor);
	assert(menu.context == Input::InputContext::MainMenu);
	assert(menu.captureMask.keyboard);
	assert(menu.captureMask.mouseButtons);
	assert(menu.captureMask.controller);
	assert(menu.dispatchMouseToMyGUI);

	io.WantCaptureMouse = true;
	const UIInputRoute imguiGameplay = UIInputRouter::Resolve(true, GameplayManager::GameState::Playing, FrontEndMode::EngineEditor);
	assert(imguiGameplay.context == Input::InputContext::Gameplay);
	assert(imguiGameplay.captureMask.mouseButtons);
	assert(!imguiGameplay.dispatchMouseToMyGUI);

	io.WantCaptureMouse = false;
	const UIInputRoute preview = UIInputRouter::Resolve(false, GameplayManager::GameState::MainMenu, FrontEndMode::GameGUICreator);
	assert(preview.context == Input::InputContext::GameGUIPreview);
	assert(preview.dispatchMouseToMyGUI);

	ImGui::DestroyContext();
	return 0;
}
