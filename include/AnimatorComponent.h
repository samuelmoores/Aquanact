#pragma once

#include <memory>

#include <assimp/scene.h>

#include "Component.h"
#include "Animator.h"
#include "Mesh.h"

class Object3D;

class AnimatorComponent : public Component {
public:
	AnimatorComponent(Mesh* mesh);

	const char* Name() const override;
	void Update(Object3D& owner, float dt) override;

	Animator* GetAnimator();
	const Animator* GetAnimator() const;

private:
	std::unique_ptr<Animator> m_animator;
};
