#include "Engine/SimulationManager.h"

#include "Engine/GameplayManager.h"
#include "Engine/Input.h"
#include "Engine/RenderManager.h"
#include "Engine/Window.h"
#include "Engine/Globals.h"

void SimulationManager::run(Window& window, Input& input, GameplayManager& gameplayManager, RenderManager& renderManager, EngineState& engineState)
{
	while (!window.ShouldClose())
	{
		input.Update();
		if (engineState.IsGameMode() || gameplayManager.ShouldUpdateInEditor())
		{
			gameplayManager.Update(input.DeltaTime());
		}

		renderManager.Loop();
	}
}
