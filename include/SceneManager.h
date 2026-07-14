#pragma once

#include <memory>
#include <vector>

class Object3D;

class SceneManager {
public:
	SceneManager() = default;

	void Clear();
	Object3D* AddObject(std::unique_ptr<Object3D> object);
	const std::vector<std::unique_ptr<Object3D>>& Objects() const;

private:
	std::vector<std::unique_ptr<Object3D>> m_objects;
};
