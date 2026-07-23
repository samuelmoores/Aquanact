#include "Engine/SceneManager.h"

#include "Engine/Object3D.h"

void SceneManager::Clear()
{
	m_objects.clear();
}

Object3D* SceneManager::AddObject(std::unique_ptr<Object3D> object)
{
	if (!object)
	{
		return nullptr;
	}

	Object3D* rawObject = object.get();
	m_objects.push_back(std::move(object));
	return rawObject;
}

const std::vector<std::unique_ptr<Object3D>>& SceneManager::Objects() const
{
	return m_objects;
}

