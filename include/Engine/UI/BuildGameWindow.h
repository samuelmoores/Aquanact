#pragma once

#include <string>

class BuildGameWindow
{
public:
	void Draw(bool& popupRequested);

private:
	char m_buildPath[512] = "C:\\dev\\Aquanact\\out\\package";
	bool m_requestedBuild = false;
	std::string m_statusMessage;
};
