#pragma once

#include <string>

class Scene;
class SceneManager;

class ComponentDeletionWindow
{
public:
	void Draw(SceneManager& sceneManager, bool& popupRequested);

private:
	static std::size_t RemoveLiveComponentsFromScene(Scene* scene, const std::string& componentName);
	static std::size_t CountLiveComponentsInScene(const Scene* scene, const std::string& componentName);
	static std::size_t RemoveLiveComponentsFromAllScenes(const SceneManager& sceneManager, const std::string& componentName);
	static std::size_t CountLiveComponentsInAllScenes(const SceneManager& sceneManager, const std::string& componentName);
	static void DeleteComponentType(SceneManager& sceneManager, const std::string& componentName);

	std::string m_selectedComponent;
};
