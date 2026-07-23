#pragma once

#include <memory>
#include <string>
#include <vector>

#include <assimp/scene.h>

#include "Engine/Component.h"
#include "Engine/Animator.h"
#include "Engine/Mesh.h"

class Entity;

class AnimatorComponent : public Component {
public:
	struct State {
		std::string name;
		int clipIndex = -1;
	};

	struct Transition {
		std::string from;
		std::string to;
		float blendSeconds = 0.33f;
	};

	AnimatorComponent(Mesh* mesh);

	const char* Name() const override;
	void Update(Entity& owner, float dt) override;

	Animator* GetAnimator();
	const Animator* GetAnimator() const;
	const std::vector<State>& States() const;
	const std::vector<Transition>& Transitions() const;
	const std::string& CurrentState() const;
	const std::string& DesiredState() const;
	void SetDesiredState(const std::string& stateName);
	bool AddState(std::string name, int clipIndex);
	bool AddTransition(std::string from, std::string to, float blendSeconds);

private:
	void ActivateState(const std::string& stateName);
	const Transition* FindTransition(const std::string& from, const std::string& to) const;
	const State* FindState(const std::string& name) const;

	std::unique_ptr<Animator> m_animator;
	std::vector<State> m_states;
	std::vector<Transition> m_transitions;
	std::string m_currentState;
	std::string m_desiredState;
};

