#pragma once

#include <string>

struct InputMapUiState
{
	std::string selectedAction = "Move";
	char newActionName[64] = "";
	bool addActionPopupRequested = false;
	std::string statusMessage;
};

class InputMapWindow
{
public:
	void SetOpen(bool open) { m_open = open; }
	bool IsOpen() const { return m_open; }
	void Draw();

private:
	bool m_open = false;
	InputMapUiState m_ui;
};
