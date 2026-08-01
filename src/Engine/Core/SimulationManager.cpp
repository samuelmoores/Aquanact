#include "Engine/Core/SimulationManager.h"

#include "Engine/Core/GameplayManager.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/Root.h"

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
