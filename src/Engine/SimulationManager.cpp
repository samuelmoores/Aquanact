#include "Engine/SimulationManager.h"

#include "Engine/GameplayManager.h"
#include "Engine/Input.h"
#include "Engine/RenderManager.h"
#include "Engine/Window.h"
#include "Engine/Root.h"

void SimulationManager::run(Window& window, Input& input, GameplayManager& gameplayManager, RenderManager& renderManager, EngineState& engineState)
{
	while (!window.ShouldClose())
	{
		input.Update();
		if (engineState.IsGameMode())
		{
			gameplayManager.Update(input.DeltaTime(), Root::Current().FrontEnd(), Root::Current().Debugger(), engineState);
		}

		renderManager.Loop(Root::Current().FrontEnd(), Root::Current().Files(), Root::Current().Levels(), Root::Current().Projects(), Root::Current().Debugger(), input, window, engineState);
	}
}
