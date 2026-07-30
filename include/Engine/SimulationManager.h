#pragma once

class Window;
class Input;
class GameplayManager;
class RenderManager;
class EngineState;

class SimulationManager {
public:
	void run(Window& window, Input& input, GameplayManager& gameplayManager, RenderManager& renderManager, EngineState& engineState);
};
