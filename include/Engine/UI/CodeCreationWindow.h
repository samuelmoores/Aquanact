#pragma once

#include <string>

class ProjectManager;
class Window;

class CodeCreationWindow
{
public:
	void Draw(Window* window, bool& popupRequested, ProjectManager& projectManager);

private:
	static std::string NormalizeClassName(const std::string& input);
	void SaveNewClassConfiguration(const std::string& className, bool attachToExistingEntity,
		const std::string& targetEntityName, bool attachToExistingInstance,
		const std::string& targetInstanceName);

	char m_newCodeFileName[128] = "";
	std::string m_statusMessage;
	std::string m_createAndBuildClassName;
	int m_selectedEntityIndex = 0;
	int m_selectedInstanceIndex = 0;
	int m_targetKind = 0;
	bool m_createAndBuildPopupRequested = false;
};
