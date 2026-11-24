#pragma once

#include "Engine/Core/Component.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/ShaderProgram.h"

#include <glm/glm.hpp>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class RigidBodyPreset : int
{
	Default = 0,
	Bouncy = 1,
	Hard = 2,
	Slippery = 3,
	Rubber = 4
};

enum class RollingPreset : int
{
	Natural = 0,
	Fast = 1,
	LongRoll = 2,
	QuickStop = 3
};

class EntityStateMachine;
class CutsceneAnimator;
class Controller;

enum class PhysicsColliderShape
{
	Box = 0,
	Capsule = 1,
	Convex = 2,
	Sphere = 3
};

struct SubmeshColliderSettings
{
	bool enabled = true;
	PhysicsColliderShape shape = PhysicsColliderShape::Box;
	// Untouched submesh settings follow the entity-level shape, including when
	// legacy project data changes that default after entity construction.
	bool useEntityDefaultShape = true;
};

class Entity
{
public:
	// Entities own their components and drive their lifecycle in a fixed order.
	Entity(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces);
	Entity(const char* modelFile, bool addDefaultComponents = true, bool loadAnimations = true);
	explicit Entity(std::string name = "Entity", bool addDefaultComponents = true);
	virtual ~Entity();

	const std::string& Name() const { return m_name; }
	void SetName(std::string name) { m_name = std::move(name); }
	unsigned int Id() const { return m_id; }
	void SetId(unsigned int id);

	// Entity-level bindables are separate from component bindables so the UI
	// can query core transform or gameplay state directly from the entity.
	virtual const char* TypeName() const { return "Entity"; }
	virtual std::vector<BindableMember> GetBindableMembers() const { return {}; }
	virtual bool TryGetBindableValue(const std::string&, float&) const { return false; }
	virtual std::vector<BindableEvent> GetBindableEvents() const { return {}; }
	virtual void startUp();
	virtual void FirstFrame() {}
	virtual void OnTriggerEnter(Entity& triggerOwner);
	virtual void OnTriggerExit(Entity& triggerOwner);
	void FirstFrameComponents();

	Mesh* GetMesh();
	bool RefreshAnimationsFromDisk();
	ShaderProgram* GetShader();
	EntityStateMachine* GetEntityState();
	CutsceneAnimator* GetCutsceneAnimator();
	Controller* GetController();
	// These return snapshots so callers can iterate safely while other code may
	// still be mutating the underlying owned component list.
	std::vector<Component*> Components();
	std::vector<const Component*> Components() const;
	Component* GetComponentByName(const std::string& name);
	const Component* GetComponentByName(const std::string& name) const;

	template<typename T>
	T* GetComponent();
	template<typename T>
	const T* GetComponent() const;
	template<typename T, typename... Args>
	T* AddComponent(Args&&... args);
	// Ownership transfer is explicit: the entity takes responsibility for the
	// component and sets the owner pointer before storing it.
	Component* AddComponent(std::unique_ptr<Component> component);
	bool RemoveComponent(Component* component);
	template<typename T>
	bool RemoveComponent();

	void UpdateComponents(float dt);
	void UpdateControllers(float dt);
	void UpdateNonControllerComponents(float dt);
	glm::mat4 BuildModelMatrix();
	void Rotate(glm::vec3 delta);
	void RotateAroundWorldAxis(const glm::vec3& axis, float angle);
	void Move(glm::vec3 delta);
	void Translate(glm::vec3 delta);
	void Scale(glm::vec3 delta);
	void SetScale(glm::vec3 scale);
	void updateMeshAABB(glm::vec3 delta);
	bool intersectsRayMesh(glm::vec3 origin, glm::vec3& direction);
	bool skinned();

	std::string SourcePath() const;
	glm::vec3 Position() const;
	glm::vec3 WorldPosition();
	glm::vec3 WorldCenterPosition();
	bool WorldAABB(glm::vec3& minBounds, glm::vec3& maxBounds);
	glm::vec3 InitialWorldCenterPosition() const;
	glm::vec3 DefaultPosition() const;
	glm::vec3 DefaultRotation() const;
	glm::vec3 Rotation() const;
	glm::vec3 Scale() const;
	void SetRotation(glm::vec3 newRotation);
	// Attachments are transform relationships only; scene ownership remains
	// unchanged. While attached, Position/Rotation/Scale are local to Parent().
	// The default local transform places the child at the parent's origin.
	bool AttachTo(Entity& parent, glm::vec3 localPosition = glm::vec3(0.0f),
		glm::vec3 localRotation = glm::vec3(0.0f));
	// Detaching normally preserves the child's current world transform, which is
	// useful for releasing a projectile without a visible teleport.
	void Detach(bool preserveWorldTransform = true);
	bool IsAttached() const { return m_parent != nullptr; }
	Entity* Parent() const { return m_parent; }
	void SetSubmeshComponentOwner(unsigned int parentEntityId, int submeshIndex)
	{
		m_submeshComponentParentId = parentEntityId;
		m_submeshComponentIndex = submeshIndex;
	}
	bool IsSubmeshComponentOwner() const { return m_submeshComponentParentId != 0 && m_submeshComponentIndex >= 0; }
	unsigned int SubmeshComponentParentId() const { return m_submeshComponentParentId; }
	int SubmeshComponentIndex() const { return m_submeshComponentIndex; }
	void SetDefaultPosition(glm::vec3 position);
	void SetDefaultRotation(glm::vec3 rotation);
	void ResetToDefaultPosition();
	void ResetToDefaultRotation();

	void SetIgnoreCameraCollision(bool ignore) { m_ignoreCameraCollision = ignore; }
	bool IgnoreCameraCollision() const { return m_ignoreCameraCollision; }
	void SetBlocksCollision(bool blocks) { m_blocksCollision = blocks; }
	bool BlocksCollision() const { return m_blocksCollision; }
	void SetBlocksCameraView(bool blocks) { m_blocksCameraView = blocks; }
	bool BlocksCameraView() const { return m_blocksCameraView; }
	void SetRigidBody(bool enabled)
	{
		m_rigidBody = enabled;
		if (!enabled)
		{
			m_linearVelocity = glm::vec3(0.0f);
			m_angularVelocity = glm::vec3(0.0f);
		}
	}
	bool IsRigidBody() const { return m_rigidBody; }
	RigidBodyPreset GetRigidBodyPreset() const { return m_rigidBodyPreset; }
	void SetRigidBodyPreset(RigidBodyPreset preset) { m_rigidBodyPreset = preset; }
	float Restitution() const
	{
		switch (m_rigidBodyPreset)
		{
		case RigidBodyPreset::Bouncy: return 0.85f;
		case RigidBodyPreset::Hard: return 0.10f;
		case RigidBodyPreset::Slippery: return 0.05f;
		case RigidBodyPreset::Rubber: return 0.70f;
		default: return 0.35f;
		}
	}
	float MinimumBounceSpeed() const
	{
		switch (m_rigidBodyPreset)
		{
		case RigidBodyPreset::Bouncy: return 5.0f;
		case RigidBodyPreset::Rubber: return 10.0f;
		default: return 50.0f;
		}
	}
	float Friction() const
	{
		switch (m_rigidBodyPreset)
		{
		case RigidBodyPreset::Bouncy: return 0.35f;
		case RigidBodyPreset::Hard: return 0.40f;
		case RigidBodyPreset::Slippery: return 0.03f;
		case RigidBodyPreset::Rubber: return 0.90f;
		default: return 0.50f;
		}
	}
	float LinearDamping() const
	{
		switch (m_rigidBodyPreset)
		{
		case RigidBodyPreset::Bouncy: return 0.01f;
		case RigidBodyPreset::Hard: return 0.08f;
		case RigidBodyPreset::Slippery: return 0.005f;
		case RigidBodyPreset::Rubber: return 0.05f;
		default: return 0.02f;
		}
	}
	float AngularDamping() const
	{
		switch (m_rigidBodyPreset)
		{
		case RigidBodyPreset::Bouncy: return 0.01f;
		case RigidBodyPreset::Hard: return 0.10f;
		case RigidBodyPreset::Slippery: return 0.005f;
		case RigidBodyPreset::Rubber: return 0.08f;
		default: return 0.02f;
		}
	}
	RollingPreset GetRollingPreset() const { return m_rollingPreset; }
	void SetRollingPreset(RollingPreset preset) { m_rollingPreset = preset; }
	float RollingResistance() const
	{
		switch (m_rollingPreset)
		{
		case RollingPreset::Fast: return 0.01f;
		case RollingPreset::LongRoll: return 0.002f;
		case RollingPreset::QuickStop: return 2.0f;
		default: return 0.05f;
		}
	}
	const glm::vec3& LinearVelocity() const { return m_linearVelocity; }
	void SetLinearVelocity(const glm::vec3& velocity) { m_linearVelocity = velocity; }
	const glm::vec3& AngularVelocity() const { return m_angularVelocity; }
	void SetAngularVelocity(const glm::vec3& velocity) { m_angularVelocity = velocity; }
	void ApplyImpulse(const glm::vec3& impulse)
	{
		if (m_rigidBody && !GetController())
			m_linearVelocity += impulse;
	}
	// Launch enables non-controller rigid-body simulation and sets velocity in world units/second.
	void Launch(const glm::vec3& velocity)
	{
		if (GetController()) return;
		m_rigidBody = true;
		m_linearVelocity = velocity;
	}
	void SetShowPhysicsBoundingBox(bool show) { m_showPhysicsBoundingBox = show; }
	bool ShowPhysicsBoundingBox() const { return m_showPhysicsBoundingBox; }
	void SetPhysicsColliderShape(PhysicsColliderShape shape);
	PhysicsColliderShape GetPhysicsColliderShape() const { return m_physicsColliderShape; }
	void SetCapsuleRadiusOverride(float radius);
	float CapsuleRadiusOverride() const { return m_capsuleRadiusOverride; }
	void SetUseBaseMeshCollider(bool enabled) { m_useBaseMeshCollider = enabled; }
	bool UseBaseMeshCollider() const { return m_useBaseMeshCollider; }
	void SyncSubmeshColliderSettings();
	const std::vector<SubmeshColliderSettings>& GetSubmeshColliderSettings();
	SubmeshColliderSettings GetSubmeshColliderSettings(std::size_t submeshIndex) const;
	void SetSubmeshColliderEnabled(std::size_t submeshIndex, bool enabled);
	void SetSubmeshColliderShape(std::size_t submeshIndex, PhysicsColliderShape shape);
	void SetSubmeshColliderSettings(std::size_t submeshIndex, const SubmeshColliderSettings& settings);
	bool HasEntityState() const;

protected:
	Mesh* m_mesh = nullptr;
	ShaderProgram m_shader;
	glm::vec3 m_position{0.0f};
	glm::vec3 m_rotation{0.0f};
	glm::vec3 m_scale{1.0f};
	glm::vec3 m_initialWorldCenter{0.0f};
	glm::vec3 m_defaultPosition{0.0f};
	glm::vec3 m_defaultRotation{0.0f};
	bool m_skinned = false;
	bool m_blocksCollision = true;
	bool m_ignoreCameraCollision = false;
	bool m_blocksCameraView = true;
	bool m_rigidBody = false;
	RigidBodyPreset m_rigidBodyPreset = RigidBodyPreset::Default;
	RollingPreset m_rollingPreset = RollingPreset::Natural;
	glm::vec3 m_linearVelocity{ 0.0f };
	glm::vec3 m_angularVelocity{ 0.0f };
	bool m_showPhysicsBoundingBox = false;
	PhysicsColliderShape m_physicsColliderShape = PhysicsColliderShape::Box;
	float m_capsuleRadiusOverride = 0.0f;
	bool m_useBaseMeshCollider = false;
	std::vector<SubmeshColliderSettings> m_submeshColliderSettings;
	std::string m_name;
	std::string m_sourcePath;
	unsigned int m_id = 0;
	std::vector<std::unique_ptr<Component>> m_components;
	Entity* m_parent = nullptr;
	unsigned int m_submeshComponentParentId = 0;
	int m_submeshComponentIndex = -1;
	std::vector<Entity*> m_children;

public:
	static inline std::vector<Vertex3D> cubeVertices = {
		{ {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
		{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
		{ {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
		{ {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
		{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
		{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
		{ {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
		{ { -0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
		{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
		{ { 0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
		{ { 0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
		{ { 0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
		{ { 0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
		{ {  0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
		{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
		{ {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
		{ {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
		{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
		{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
		{ {  0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
	};
	static inline std::vector<uint32_t> cubeFaces = {
		0, 1, 2, 0, 2, 3,
		4, 5, 6, 4, 6, 7,
		8, 9,10, 8,10,11,
		12,13,14, 12,14,15,
		16,17,18, 16,18,19,
		20,21,22, 20,22,23
	};
};

template<typename T>
T* Entity::GetComponent()
{
	for (auto& component : m_components)
	{
		if (auto* typed = dynamic_cast<T*>(component.get()))
		{
			return typed;
		}
	}
	return nullptr;
}

template<typename T>
const T* Entity::GetComponent() const
{
	for (const auto& component : m_components)
	{
		if (auto* typed = dynamic_cast<T*>(component.get()))
		{
			return typed;
		}
	}
	return nullptr;
}

template<typename T, typename... Args>
T* Entity::AddComponent(Args&&... args)
{
	auto component = std::make_unique<T>(std::forward<Args>(args)...);
	T* raw = component.get();
	raw->SetOwner(this);
	m_components.push_back(std::move(component));
	return raw;
}

template<typename T>
bool Entity::RemoveComponent()
{
	for (auto it = m_components.begin(); it != m_components.end(); ++it)
	{
		if (dynamic_cast<T*>(it->get()))
		{
			m_components.erase(it);
			return true;
		}
	}
	return false;
}

