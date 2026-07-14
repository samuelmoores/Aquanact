#pragma once

#include <memory>
#include <vector>

class Object3D;

class LevelManager {
public:
	LevelManager() = default;

	void Clear();
	Object3D* AddObject(std::unique_ptr<Object3D> object);

	const std::vector<std::unique_ptr<Object3D>>& Objects() const;

private:
	std::vector<std::unique_ptr<Object3D>> m_objects;
};
