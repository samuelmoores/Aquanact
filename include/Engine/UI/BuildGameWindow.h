#pragma once

#include <string>

class BuildGameWindow
{
public:
	void Draw(bool& popupRequested);

private:
	char m_buildPath[512] = {};
	bool m_initializedPath = false;
	bool m_requestedBuild = false;
	std::string m_statusMessage;
};
