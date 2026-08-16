#pragma once

#include "Engine/UI/EngineGuiContext.h"
#include <string>

class SceneManager;

class SceneWindow
{
public:
	void Draw(const EngineGuiFrameContext& context, bool& open, bool& entityWindowOpen) const;
	void DrawNewLevelPopup(SceneManager& sceneManager, bool& requested);

private:
	char m_newLevelName[128] = "Level1";
	std::string m_newLevelStatusMessage;
};
