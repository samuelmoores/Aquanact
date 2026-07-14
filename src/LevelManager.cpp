#include "LevelManager.h"

#include "Object3D.h"

void LevelManager::Clear()
{
	m_objects.clear();
}

Object3D* LevelManager::AddObject(std::unique_ptr<Object3D> object)
{
	if (!object)
	{
		return nullptr;
	}

	Object3D* rawObject = object.get();
	m_objects.push_back(std::move(object));
	return rawObject;
}

const std::vector<std::unique_ptr<Object3D>>& LevelManager::Objects() const
{
	return m_objects;
}
