#include "Engine/Core/Debug.h"

#include "Engine/Core/Axis.h"
#include "Engine/UI/EngineGUI.h"
#include "Engine/Core/Grid.h"
#include "Engine/Core/Line.h"
#include "Engine/Core/Camera.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/GameplayManager.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUIManager.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/LightingManager.h"
#include "Engine/Core/FrameAllocator.h"
#include "Engine/Core/FrameProfiler.h"
#include "Engine/Core/GLHeaders.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/PathedCamera.h"
#include "Engine/Core/TriggerSphere.h"
#include "Engine/Core/CameraPathData.h"
#include "Engine/UI/CameraPathCreator.h"
#include "Engine/Core/Mesh.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Engine/Core/PhysicsWorld.h"

#include <imgui.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <filesystem>
#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

#ifdef _WIN32
#include <Windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

namespace {
	std::vector<LineVertex3D> MakeBoxFaceVertices(const glm::vec3& minimum, const glm::vec3& maximum, int axis, float direction, const glm::vec3& color)
	{
		const float coordinate = direction > 0.0f ? maximum[axis] : minimum[axis];
		glm::vec3 a, b, c, d;
		if (axis == 0) { a = { coordinate, minimum.y, minimum.z }; b = { coordinate, maximum.y, minimum.z }; c = { coordinate, maximum.y, maximum.z }; d = { coordinate, minimum.y, maximum.z }; }
		else if (axis == 1) { a = { minimum.x, coordinate, minimum.z }; b = { maximum.x, coordinate, minimum.z }; c = { maximum.x, coordinate, maximum.z }; d = { minimum.x, coordinate, maximum.z }; }
		else { a = { minimum.x, minimum.y, coordinate }; b = { maximum.x, minimum.y, coordinate }; c = { maximum.x, maximum.y, coordinate }; d = { minimum.x, maximum.y, coordinate }; }
		const auto makeVertex = [&color](const glm::vec3& p) { return LineVertex3D{ p.x, p.y, p.z, color.r, color.g, color.b }; };
		return { makeVertex(a), makeVertex(b), makeVertex(b), makeVertex(c), makeVertex(c), makeVertex(d), makeVertex(d), makeVertex(a) };
	}
	const auto g_programStartTime = std::chrono::high_resolution_clock::now();
	std::vector<LineVertex3D> MakeFaceArrowVertices(const glm::vec3& center, const glm::vec3& normal, float length, const glm::vec3& color)
	{
		const glm::vec3 tip = center + normal * length;
		glm::vec3 tangent = glm::cross(normal, glm::vec3(0, 1, 0));
		if (glm::dot(tangent, tangent) < 0.001f) tangent = glm::cross(normal, glm::vec3(1, 0, 0));
		tangent = glm::normalize(tangent) * (length * 0.16f);
		const glm::vec3 side = glm::normalize(glm::cross(normal, tangent)) * (length * 0.16f);
		auto v = [&color](const glm::vec3& p) { return LineVertex3D{p.x,p.y,p.z,color.r,color.g,color.b}; };
		return {v(center),v(tip), v(tip),v(tip-tangent), v(tip),v(tip+tangent), v(tip),v(tip-side), v(tip),v(tip+side)};
	}

	std::string FormatBindableValueForDebug(float value)
	{
		std::ostringstream stream;
		stream << std::fixed << std::setprecision(3) << value;
		return stream.str();
	}

	struct DebugBindableGroup
	{
		std::string label;
		const Component* component = nullptr;
		std::vector<BindableMember> members;
	};

	std::vector<DebugBindableGroup> CollectBindableGroups(const Entity& entity)
	{
		std::vector<DebugBindableGroup> groups;
		auto addGroup = [&groups](std::string label, const Component* component, std::vector<BindableMember> members)
		{
			if (!members.empty())
			{
				groups.push_back({ std::move(label), component, std::move(members) });
			}
		};

		addGroup("Entity", nullptr, entity.GetBindableMembers());
		for (const Component* component : entity.Components())
		{
			if (!component)
			{
				continue;
			}
			addGroup(component->Name(), component, component->GetBindableMembers());
		}
		return groups;
	}

	void DrawBindableGroup(const DebugBindableGroup& group, const Entity& entity)
	{
		if (!ImGui::TreeNode(group.label.c_str()))
		{
			return;
		}

		for (const BindableMember& member : group.members)
		{
			float value = 0.0f;
			const bool hasValue = group.component
				? group.component->TryGetBindableValue(member.name, value)
				: entity.TryGetBindableValue(member.name, value);
			ImGui::Text("%s: %s", member.displayName.empty() ? member.name.c_str() : member.displayName.c_str(),
				hasValue ? FormatBindableValueForDebug(value).c_str() : "<unavailable>");
		}

		ImGui::TreePop();
	}

	std::vector<LineVertex3D> MakeWireSphereVertices(const glm::vec3& color)
	{
		constexpr int segments = 48;
		constexpr float pi = 3.14159265358979323846f;
		std::vector<LineVertex3D> vertices;
		vertices.reserve(segments * 2 * 3);

		auto addVertex = [&](const glm::vec3& position) {
			vertices.push_back({ position.x, position.y, position.z, color.r, color.g, color.b });
		};

		for (int i = 0; i < segments; ++i)
		{
			const float a0 = (static_cast<float>(i) / static_cast<float>(segments)) * 2.0f * pi;
			const float a1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * 2.0f * pi;

			addVertex(glm::vec3(std::cos(a0), std::sin(a0), 0.0f));
			addVertex(glm::vec3(std::cos(a1), std::sin(a1), 0.0f));

			addVertex(glm::vec3(std::cos(a0), 0.0f, std::sin(a0)));
			addVertex(glm::vec3(std::cos(a1), 0.0f, std::sin(a1)));

			addVertex(glm::vec3(0.0f, std::cos(a0), std::sin(a0)));
			addVertex(glm::vec3(0.0f, std::cos(a1), std::sin(a1)));
		}

		return vertices;
	}

	void BuildVerticalCapsule(const glm::vec3& boxMin, const glm::vec3& boxMax,
		glm::vec3& base, glm::vec3& tip, float& radius)
	{
		const glm::vec3 center = (boxMin + boxMax) * 0.5f;
		const glm::vec3 halfExtents = (boxMax - boxMin) * 0.5f;
		radius = std::min(std::min(halfExtents.x, halfExtents.z), halfExtents.y);
		radius = std::max(radius, 0.001f);
		const float baseY = boxMin.y + radius;
		const float tipY = boxMax.y - radius;
		base = glm::vec3(center.x, std::min(baseY, tipY), center.z);
		tip = glm::vec3(center.x, std::max(baseY, tipY), center.z);
	}

	bool BuildPhysicsCapsule(const Entity& object, const glm::vec3& boxMin,
		const glm::vec3& boxMax, glm::vec3& base, glm::vec3& tip, float& radius)
	{
		const PhysicsWorld& world = PhysicsWorld::Instance();
		const ColliderHandle handle = world.Find(object);
		if (handle == InvalidColliderHandle || handle >= world.Colliders().size())
		{
			return false;
		}

		const PhysicsCollider& collider = world.Colliders()[handle];
		if (collider.shape != PhysicsColliderShape::Capsule || collider.capsuleRadius <= 0.0f)
		{
			return false;
		}

		const glm::vec3 center = (boxMin + boxMax) * 0.5f;
		radius = collider.capsuleRadius;
		base = center - glm::vec3(0.0f, collider.capsuleHalfLength, 0.0f);
		tip = center + glm::vec3(0.0f, collider.capsuleHalfLength, 0.0f);
		return true;
	}

	std::vector<LineVertex3D> MakeWireCapsuleVertices(
		const glm::vec3& base, const glm::vec3& tip, float radius, const glm::vec3& color)
	{
		constexpr int segments = 12;
		constexpr int hemisphereRings = 3;
		constexpr float pi = 3.14159265358979323846f;
		std::vector<std::vector<glm::vec3>> rings;
		rings.reserve(hemisphereRings * 2 + 2);

		auto addRing = [&](float centerY, float angle)
		{
			std::vector<glm::vec3> ring;
			ring.reserve(segments);
			const float ringRadius = std::cos(angle) * radius;
			const float y = centerY + std::sin(angle) * radius;
			const glm::vec3 center = (base + tip) * 0.5f;
			for (int segment = 0; segment < segments; ++segment)
			{
				const float a = static_cast<float>(segment) / static_cast<float>(segments) * 2.0f * pi;
				ring.emplace_back(center.x + std::cos(a) * ringRadius, y, center.z + std::sin(a) * ringRadius);
			}
			rings.push_back(std::move(ring));
		};

		for (int ring = 0; ring <= hemisphereRings; ++ring)
		{
			const float angle = -0.5f * pi + (0.5f * pi * static_cast<float>(ring) / hemisphereRings);
			addRing(base.y, angle);
		}
		for (int ring = 0; ring <= hemisphereRings; ++ring)
		{
			const float angle = 0.5f * pi * static_cast<float>(ring) / hemisphereRings;
			addRing(tip.y, angle);
		}

		std::vector<LineVertex3D> vertices;
		vertices.reserve((rings.size() * segments + (rings.size() - 1) * segments) * 2);
		auto addLine = [&](const glm::vec3& a, const glm::vec3& b)
		{
			vertices.push_back({ a.x, a.y, a.z, color.r, color.g, color.b });
			vertices.push_back({ b.x, b.y, b.z, color.r, color.g, color.b });
		};

		for (const auto& ring : rings)
		{
			for (int segment = 0; segment < segments; ++segment)
			{
				addLine(ring[segment], ring[(segment + 1) % segments]);
			}
		}
		for (std::size_t ring = 1; ring < rings.size(); ++ring)
		{
			for (int segment = 0; segment < segments; ++segment)
			{
				addLine(rings[ring - 1][segment], rings[ring][segment]);
			}
		}
		return vertices;
	}

	std::vector<LineVertex3D> MakeWireConvexVertices(const Entity& object, const glm::vec3& color)
	{
		const Mesh* mesh = const_cast<Entity&>(object).GetMesh();
		if (!mesh)
		{
			return {};
		}

		const auto& vertices = mesh->Vertices();
		const auto& faces = mesh->Faces();
		const glm::mat4 model = const_cast<Entity&>(object).BuildModelMatrix();
		std::vector<LineVertex3D> result;
		result.reserve(faces.size() * 2);
		auto addLine = [&](const glm::vec3& a, const glm::vec3& b)
		{
			result.push_back({ a.x, a.y, a.z, color.r, color.g, color.b });
			result.push_back({ b.x, b.y, b.z, color.r, color.g, color.b });
		};
		for (std::size_t i = 0; i + 2 < faces.size(); i += 3)
		{
			if (faces[i] >= vertices.size() || faces[i + 1] >= vertices.size() || faces[i + 2] >= vertices.size())
			{
				continue;
			}
			const glm::vec3 a = glm::vec3(model * glm::vec4(vertices[faces[i]].position, 1.0f));
			const glm::vec3 b = glm::vec3(model * glm::vec4(vertices[faces[i + 1]].position, 1.0f));
			const glm::vec3 c = glm::vec3(model * glm::vec4(vertices[faces[i + 2]].position, 1.0f));
			addLine(a, b);
			addLine(b, c);
			addLine(c, a);
		}
		return result;
	}
}

void Debug::startUp()
{
	// Packaged games do not expose editor diagnostics windows, so retain a
	// persistent log beside the executable for runtime investigation.
	try
	{
		const std::filesystem::path logDirectory =
			Root::Current().FileSystemRef().ExecutableDirectory() / "logs";
		std::filesystem::create_directories(logDirectory);
		m_runtimeLog.open(logDirectory / "aquanact-runtime.txt", std::ios::out | std::ios::trunc);
	}
	catch (...)
	{
		// Logging must never prevent the engine from starting.
	}

	LogTagged("Runtime", Root::Current().State().IsGameMode()
		? "Started packaged/game runtime" : "Started editor runtime");
	LogBuildInfo();
	VerifyDependencies();

	if (!Root::Current().State().IsEditorMode())
	{
		return;
	}

	// The debug subsystem owns the editor overlay primitives and records
	// early build/dependency diagnostics before the rest of the app starts running.
	RebuildAxis();
	RebuildGrid();
}

void Debug::shutDown()
{
	// Release overlay helpers first so they cannot outlive the renderer or Scene data.
	delete m_axis;
	m_axis = nullptr;
	delete m_grid;
	m_grid = nullptr;
	for (Line* sphere : m_pointLightDebugSpheres)
	{
		delete sphere;
	}
	m_pointLightDebugSpheres.clear();
	m_pointLightDebugColors.clear();
	for (Line* sphere : m_cameraPathSpheres) delete sphere;
	for (Line* sphere : m_cameraPathTriggerSpheres) delete sphere;
	for (Line* segment : m_cameraPathSegments) delete segment;
	m_cameraPathSpheres.clear();
	m_cameraPathTriggerSpheres.clear();
	m_cameraPathSegments.clear();
	for (auto& entry : m_triggerSpheres) delete entry.second;
	m_triggerSpheres.clear();
	ClearEntityBoundingBoxes();
	for (Line* volume : m_levelColliderBounds) delete volume;
	m_levelColliderBounds.clear();
	for (Line* gizmo : m_levelColliderFaceGizmos) delete gizmo;
	m_levelColliderFaceGizmos.clear();
	for (Line* highlight : m_levelColliderFaceHighlights) delete highlight;
	m_levelColliderFaceHighlights.clear();
	m_levelColliderObjects.clear();
	delete m_cameraCollisionSphere;
	m_cameraCollisionSphere = nullptr;
	m_logMessages.clear();
	m_logOnceKeys.clear();
	if (m_runtimeLog.is_open())
	{
		m_runtimeLog.flush();
		m_runtimeLog.close();
	}
}

void Debug::ClearEntityBoundingBoxes()
{
	for (Line* box : m_entityBoundingBoxes)
	{
		delete box;
	}
	m_entityBoundingBoxes.clear();
	m_entityBoundingBoxObjects.clear();
}

void Debug::DrawCameraCollisionDebug(const Camera& camera)
{
	if (!m_showCameraCollisionDebug)
	{
		return;
	}

	return;
}

void Debug::DrawCameraPath(const Camera& camera, const CameraPathData& path, int selectedPoint)
{
	// Rebuild each editor frame so dragging a point immediately moves its sphere
	// and the connecting segments.
	if (!path.points.empty() || !m_cameraPathSpheres.empty() || !m_cameraPathTriggerSpheres.empty())
	{
		for (Line* sphere : m_cameraPathSpheres) delete sphere;
		for (Line* sphere : m_cameraPathTriggerSpheres) delete sphere;
		for (Line* segment : m_cameraPathSegments) delete segment;
		m_cameraPathSpheres.clear();
		m_cameraPathTriggerSpheres.clear();
		m_cameraPathSegments.clear();
		for (std::size_t i = 0; i < path.points.size(); ++i)
		{
			const glm::vec3 color = static_cast<int>(i) == selectedPoint
				? glm::vec3(1.0f, 0.8f, 0.1f) : glm::vec3(0.2f, 0.7f, 1.0f);
			m_cameraPathSpheres.push_back(new Line(MakeWireSphereVertices(color)));
			if (path.points[i].island)
			{
				m_cameraPathTriggerSpheres.push_back(new Line(MakeWireSphereVertices(glm::vec3(1.0f, 0.1f, 0.1f))));
			}
		}
		const int curveSamples = Root::Current().Render().GetPathedCamera().PathSamplesPerSegment();
		for (std::size_t segment = 0; segment + 1 < path.points.size(); ++segment)
		{
			// An island point is reached independently by its trigger, so it has
			// no authored curve segment from the preceding point. Keep a simple
			// straight guide so the authored relationship remains visible without
			// allocating many line objects every editor frame.
			if (path.points[segment + 1].island)
			{
				const glm::vec3 start = path.points[segment].position;
				const glm::vec3 end = path.points[segment + 1].position;
				const glm::vec3 color(0.95f, 0.65f, 0.2f);
				m_cameraPathSegments.push_back(new Line({
					{start.x, start.y, start.z, color.r, color.g, color.b},
					{end.x, end.y, end.z, color.r, color.g, color.b}}));
				continue;
			}
			const glm::vec3 color(0.2f, 0.7f, 1.0f);
			for (int sample = 0; sample < curveSamples; ++sample)
			{
				const float t0 = static_cast<float>(sample) / curveSamples;
				const float t1 = static_cast<float>(sample + 1) / curveSamples;
				const glm::vec3 a = EvaluateCameraPathSegment(path, segment, t0);
				const glm::vec3 b = EvaluateCameraPathSegment(path, segment, t1);
				m_cameraPathSegments.push_back(new Line({
					{a.x, a.y, a.z, color.r, color.g, color.b},
					{b.x, b.y, b.z, color.r, color.g, color.b}}));
			}
		}
	}
	const glm::mat4 projection = camera.GetProjectionMatrix();
	const glm::mat4 view = camera.GetViewMatrix();
	constexpr float pointRadius = 7.5f;
	for (std::size_t i = 0; i < path.points.size() && i < m_cameraPathSpheres.size(); ++i)
	{
		m_cameraPathSpheres[i]->UpdateProjection(projection);
		const float radius = static_cast<int>(i) == selectedPoint
			? pointRadius * 1.25f
			: pointRadius;
		m_cameraPathSpheres[i]->draw(view, glm::translate(glm::mat4(1.0f), path.points[i].position) * glm::scale(glm::mat4(1.0f), glm::vec3(radius)));
	}
	std::size_t triggerSphereIndex = 0;
	for (const CameraPathPoint& point : path.points)
	{
		if (!point.island || triggerSphereIndex >= m_cameraPathTriggerSpheres.size())
		{
			continue;
		}
		Line* triggerSphere = m_cameraPathTriggerSpheres[triggerSphereIndex++];
		triggerSphere->UpdateProjection(projection);
		triggerSphere->draw(view, glm::translate(glm::mat4(1.0f), point.triggerPosition) * glm::scale(glm::mat4(1.0f), glm::vec3(point.triggerRadius)));
	}
	for (std::size_t i = 0; i < m_cameraPathSegments.size(); ++i)
	{
		m_cameraPathSegments[i]->UpdateProjection(projection);
		m_cameraPathSegments[i]->draw(view);
	}
}

void Debug::RebuildGrid()
{
	// Grid settings are mutable in the editor, so rebuild the helper when size or spacing changes.
	delete m_grid;
	m_grid = new Grid(m_gridSize, m_gridSpacing);
}

void Debug::RebuildAxis()
{
	delete m_axis;
	m_axis = new Axis(m_axisLength);
}

void Debug::RebuildPointLightDebugSpheres()
{
	for (Line* sphere : m_pointLightDebugSpheres)
	{
		delete sphere;
	}
	m_pointLightDebugSpheres.clear();
	m_pointLightDebugColors.clear();

	for (const PointLight& pointLight : Root::Current().Render().Lights().PointLights())
	{
		const glm::vec3 color = glm::clamp(pointLight.color, glm::vec3(0.0f), glm::vec3(1.0f));
		m_pointLightDebugSpheres.push_back(new Line(MakeWireSphereVertices(color)));
		m_pointLightDebugColors.push_back(color);
	}
}

void Debug::draw(const Camera& camera, const EngineGUI& gui)
{
	// This runs only in editor mode and is responsible for the visible debugging overlay.
	// It intentionally reads current frame data instead of caching renderer state itself.
	static bool firstFrame = true;
	if (firstFrame)
	{
		firstFrame = false;
		m_lastFps = 0.0f;
		if (m_startupToFirstDrawMs < 0.0)
		{
			const auto elapsed = std::chrono::high_resolution_clock::now() - g_programStartTime;
			m_startupToFirstDrawMs = std::chrono::duration<double>(elapsed).count();
		}
	}
	else
	{
		m_lastFps = static_cast<float>(Root::Current().Profiler().SmoothedFps());
	}

	auto projection = camera.GetProjectionMatrix();
	auto view = camera.GetViewMatrix();
	DrawCameraCollisionDebug(camera);
	if (m_showTriggerSpheres)
	{
		const Scene* scene = Root::Current().Scenes().ActiveLevel();
		if (scene)
		{
			for (const auto& object : scene->Objects())
			{
				if (!object) continue;
				const TriggerSphere* trigger = object->GetComponent<TriggerSphere>();
				if (!trigger || !trigger->Enabled()) continue;
				if (!m_triggerSpheres.contains(object.get()))
					m_triggerSpheres.emplace(object.get(), new Line(MakeWireSphereVertices(glm::vec3(1.0f, 0.45f, 0.1f))));
				Line* sphere = m_triggerSpheres[object.get()];
				sphere->UpdateProjection(camera.GetProjectionMatrix());
				sphere->draw(camera.GetViewMatrix(), glm::translate(glm::mat4(1.0f), object->WorldCenterPosition()) * glm::scale(glm::mat4(1.0f), glm::vec3(trigger->Radius())));
			}
		}
	}
	if (gui.ShowCameraPath())
	{
		DrawCameraPath(camera, gui.CameraPath().Data(), gui.CameraPath().SelectedPoint());
	}

	if (m_axis && gui.ShowAxis())
	{
		m_axis->UpdateProjection(projection);
		m_axis->draw(view);
	}

	if (m_grid && gui.ShowGrid())
	{
		m_grid->UpdateProjection(projection);
		m_grid->draw(view, !gui.ShowAxis());
	}

	if (m_showPointLightDebugSpheres)
	{
		const std::vector<PointLight>& pointLights = Root::Current().Render().Lights().PointLights();
		bool rebuildLightSpheres = m_pointLightDebugSpheres.size() != pointLights.size();
		if (!rebuildLightSpheres)
		{
			for (std::size_t i = 0; i < pointLights.size(); ++i)
			{
				const glm::vec3 color = glm::clamp(pointLights[i].color, glm::vec3(0.0f), glm::vec3(1.0f));
				if (m_pointLightDebugColors[i] != color)
				{
					rebuildLightSpheres = true;
					break;
				}
			}
		}

		if (rebuildLightSpheres)
		{
			RebuildPointLightDebugSpheres();
		}

		for (std::size_t i = 0; i < pointLights.size() && i < m_pointLightDebugSpheres.size(); ++i)
		{
			Line* sphere = m_pointLightDebugSpheres[i];
			if (!sphere)
			{
				continue;
			}

			const PointLight& pointLight = pointLights[i];
			const float markerRadius = std::clamp(pointLight.radius * 0.03f, 15.0f, 80.0f);
			const glm::mat4 model =
				glm::translate(glm::mat4(1.0f), pointLight.position) *
				glm::scale(glm::mat4(1.0f), glm::vec3(markerRadius));

			sphere->UpdateProjection(projection);
			glLineWidth(2.0f);
			sphere->draw(view, model);
		}
	}

	// Level-collider debug geometry follows the same overlay path as point-light
	// markers, so it remains visible independently of the physics diagnostics UI.
	const Scene* activeLevel = Root::Current().Scenes().ActiveLevel();
	std::vector<LevelCollider*> levelColliders;
	if (activeLevel)
	{
		for (const auto& collider : activeLevel->LevelColliders())
			if (collider) levelColliders.push_back(collider.get());
	}
	if (levelColliders != m_levelColliderObjects)
	{
		for (Line* volume : m_levelColliderBounds) delete volume;
		for (Line* gizmo : m_levelColliderFaceGizmos) delete gizmo;
		for (Line* highlight : m_levelColliderFaceHighlights) delete highlight;
		m_levelColliderBounds.clear();
		m_levelColliderFaceGizmos.clear();
		m_levelColliderFaceHighlights.clear();
		m_levelColliderObjects = levelColliders;
		for (LevelCollider* collider : m_levelColliderObjects)
		{
			m_levelColliderBounds.push_back(new Line(glm::vec3(0.0f), glm::vec3(0.0f)));
			m_levelColliderFaceGizmos.push_back(new Line(glm::vec3(0.0f), glm::vec3(0.0f)));
			m_levelColliderFaceHighlights.push_back(new Line(glm::vec3(0.0f), glm::vec3(0.0f)));
		}
	}
	for (std::size_t i = 0; i < m_levelColliderObjects.size(); ++i)
	{
		LevelCollider* collider = m_levelColliderObjects[i];
		if (!collider || !collider->DebugVisible()) continue;
		Line* volume = m_levelColliderBounds[i];
		glm::vec3 halfExtents = glm::abs(collider->Scale()) * 50.0f;
		if (collider->Shape() == LevelColliderShape::Plane) halfExtents.y = 0.5f;
		const glm::vec3 debugColor = collider == m_selectedLevelCollider
			? glm::vec3(0.2f, 0.9f, 1.0f)
			: glm::vec3(0.04f, 0.25f, 0.32f);
		const bool sphereShape = collider->Shape() == LevelColliderShape::Sphere;
		if (sphereShape)
		{
			volume->SetVertices(MakeWireSphereVertices(debugColor));
		}
		else if (collider->Shape() == LevelColliderShape::Capsule)
		{
			const float radius = collider->Radius();
			const float halfHeight = std::max(radius, collider->Height() * 0.5f);
			const glm::vec3 base(0.0f, -std::max(0.0f, halfHeight - radius), 0.0f);
			const glm::vec3 tip(0.0f, std::max(0.0f, halfHeight - radius), 0.0f);
			volume->SetVertices(MakeWireCapsuleVertices(base, tip, radius, debugColor));
		}
		else
		{
			// Keep box geometry in local space so the authored collider rotation is
			// visible instead of collapsing back to an axis-aligned world box.
			const float localHalfY = collider->Shape() == LevelColliderShape::Plane ? 0.0f : 50.0f;
			volume->SetBounds(glm::vec3(-50.0f, -localHalfY, -50.0f), glm::vec3(50.0f, localHalfY, 50.0f), debugColor);
		}
		volume->UpdateProjection(projection);
		glLineWidth(2.0f);
		glm::mat4 colliderModel = glm::translate(glm::mat4(1.0f), collider->Position());
		colliderModel = glm::rotate(colliderModel, collider->Rotation().z, glm::vec3(0.0f, 0.0f, 1.0f));
		colliderModel = glm::rotate(colliderModel, collider->Rotation().y, glm::vec3(0.0f, 1.0f, 0.0f));
		colliderModel = glm::rotate(colliderModel, collider->Rotation().x, glm::vec3(1.0f, 0.0f, 0.0f));
		if (sphereShape)
		{
			const glm::vec3 absoluteScale = glm::abs(collider->Scale());
			const float radius = collider->Radius() * std::max(absoluteScale.x, std::max(absoluteScale.y, absoluteScale.z));
			colliderModel = glm::scale(colliderModel, glm::vec3(radius));
		}
		else
			colliderModel = glm::scale(colliderModel, collider->Scale());
		volume->draw(view, colliderModel);
		if (m_levelColliderFaceEditMode && collider == m_selectedLevelCollider && collider->Shape() == LevelColliderShape::Box)
		{
			Line* arrows = m_levelColliderFaceGizmos[i];
			std::vector<LineVertex3D> vertices;
			const glm::vec3 axes[3] = {glm::vec3(1,0,0),glm::vec3(0,1,0),glm::vec3(0,0,1)};
			const float arrowLength = 35.0f;
			for (int axis=0; axis<3; ++axis) for (float sign : {-1.0f,1.0f})
			{
				const glm::vec3 normal = axes[axis] * sign;
				const glm::vec3 face = collider->Position() + normal * halfExtents[axis];
				const glm::vec3 color = (axis == m_selectedLevelColliderFaceAxis && sign == m_selectedLevelColliderFaceDirection) ? glm::vec3(1,0.9f,0.1f) : debugColor;
				const auto arrow = MakeFaceArrowVertices(face, normal, arrowLength, color);
				vertices.insert(vertices.end(), arrow.begin(), arrow.end());
			}
			arrows->SetVertices(std::move(vertices));
			arrows->UpdateProjection(projection);
			arrows->draw(view);
		}
		if (m_levelColliderFaceEditMode && collider == m_selectedLevelCollider && m_selectedLevelColliderFaceAxis >= 0 && collider->Shape() == LevelColliderShape::Box)
		{
			Line* highlight = m_levelColliderFaceHighlights[i];
			highlight->SetVertices(MakeBoxFaceVertices(collider->Position() - halfExtents, collider->Position() + halfExtents,
				m_selectedLevelColliderFaceAxis, m_selectedLevelColliderFaceDirection, glm::vec3(1.0f, 0.95f, 0.1f)));
			highlight->UpdateProjection(projection);
			glLineWidth(4.0f);
			highlight->draw(view);
		}
	}

	std::vector<Entity*> currentBoundingBoxObjects;
	if (activeLevel)
	{
		currentBoundingBoxObjects.reserve(activeLevel->Objects().size());
		for (const auto& object : activeLevel->Objects())
		{
			if (object)
			{
				currentBoundingBoxObjects.push_back(object.get());
			}
		}
	}
	if (currentBoundingBoxObjects != m_entityBoundingBoxObjects)
	{
		ClearEntityBoundingBoxes();
		m_entityBoundingBoxObjects = currentBoundingBoxObjects;
		for (Entity* object : m_entityBoundingBoxObjects)
		{
			m_entityBoundingBoxes.push_back(new Line(glm::vec3(0.0f), glm::vec3(0.0f)));
		}
	}

	for (std::size_t i = 0; i < m_entityBoundingBoxObjects.size(); ++i)
	{
		Entity* object = m_entityBoundingBoxObjects[i];
		if (!object || !object->ShowPhysicsBoundingBox() || !object->GetMesh())
		{
			continue;
		}

		glm::vec3 boxMin;
		glm::vec3 boxMax;
		if (!object->WorldAABB(boxMin, boxMax))
		{
			continue;
		}

		Line* box = m_entityBoundingBoxes[i];
		const glm::vec3 debugColor(1.0f, 0.7f, 0.1f);
		if (object->GetPhysicsColliderShape() == PhysicsColliderShape::Capsule)
		{
			glm::vec3 base;
			glm::vec3 tip;
			float radius = 0.0f;
			if (!BuildPhysicsCapsule(*object, boxMin, boxMax, base, tip, radius))
			{
				BuildVerticalCapsule(boxMin, boxMax, base, tip, radius);
			}
			box->SetVertices(MakeWireCapsuleVertices(base, tip, radius, debugColor));
		}
		else if (object->GetPhysicsColliderShape() == PhysicsColliderShape::Convex)
		{
			box->SetVertices(MakeWireConvexVertices(*object, debugColor));
		}
		else
		{
			box->SetBounds(boxMin, boxMax, debugColor);
		}
		box->UpdateProjection(projection);
		box->draw(view);
	}

	if (m_showLogWindow)
	{
		ImGui::Begin("Debug Log", &m_showLogWindow, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
		if (ImGui::Button("Clear Logs"))
		{
			ClearLogs();
		}
		ImGui::Separator();
		for (const std::string& message : m_logMessages)
		{
			ImGui::TextUnformatted(message.c_str());
		}
		ImGui::End();
	}

	if (m_showStatsWindow)
	{
		DrawRenderStatsWindow();
	}
}

void Debug::DrawRenderStatsWindow()
{
	RenderManager& render = Root::Current().Render();
	const auto& graphics = render.LastFrameGraphicsStats();
	const std::size_t totalDrawCalls = graphics.mainDrawCalls + graphics.shadowDrawCalls;
	const std::uint64_t totalTriangles = graphics.mainTriangles + graphics.shadowTriangles;
	const double frameMs = Root::Current().Profiler().FrameMs();
	ImGui::Begin("Render Stats", &m_showStatsWindow, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
	ImGui::Text("FPS: %.1f", Root::Current().Profiler().SmoothedFps());
	ImGui::Text("Frame time: %.3f ms", frameMs);
	constexpr double frameBudgetMs = 16.67;
	if (frameMs > frameBudgetMs)
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.2f, 1.0f), "Over 60 FPS budget by %.3f ms", frameMs - frameBudgetMs);
	else
		ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.45f, 1.0f), "Under 60 FPS budget by %.3f ms", frameBudgetMs - frameMs);
	const Scene* activeLevel = Root::Current().Scenes().ActiveLevel();
	ImGui::Text("Scene objects: %zu", activeLevel ? activeLevel->Objects().size() : 0);
	ImGui::Text("Candidate mesh buffers: %zu", render.LastFrameCommandCount());
	ImGui::Text("Skipped objects: %zu", render.LastFrameSkippedObjects());
	ImGui::Text("Camera-visible mesh buffers: %zu",
		render.LastFrameCommandCount() - render.LastFrameFrustumCulledObjects());
	ImGui::Text("Camera-frustum culled: %zu", render.LastFrameFrustumCulledObjects());
	ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.45f, 1.0f),
		"Main draw calls saved: %zu", render.LastFrameDrawCallsSaved());
	ImGui::Separator();
	ImGui::Text("Camera draw calls actually issued: %zu", graphics.mainDrawCalls);
	ImGui::Text("Shadow-map draw calls (camera-independent): %zu", graphics.shadowDrawCalls);
	ImGui::Text("Total GPU draw calls: %zu", totalDrawCalls);
	ImGui::Text("Main triangles: %llu", static_cast<unsigned long long>(graphics.mainTriangles));
	ImGui::Text("Shadow triangles: %llu", static_cast<unsigned long long>(graphics.shadowTriangles));
	ImGui::Text("Total submitted triangles: %llu", static_cast<unsigned long long>(totalTriangles));
	if (graphics.mainDrawCalls > 2000)
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.2f, 1.0f), "High main-pass draw-call count");
	else if (graphics.shadowDrawCalls > 2000)
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.2f, 1.0f), "High shadow-pass draw-call count");
	else if (totalDrawCalls > 1000)
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Elevated draw-call count");
	else
		ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.45f, 1.0f), "Normal draw-call count");
	ImGui::Separator();
	ImGui::Text("Directional shadow: %zu draws, %llu triangles",
		graphics.directionalShadowDrawCalls,
		static_cast<unsigned long long>(graphics.directionalShadowTriangles));
	const std::size_t pointLightCount = render.Lights().PointLights().size();
	for (std::size_t lightIndex = 0;
		lightIndex < pointLightCount && lightIndex < LightingManager::MaxPointLights;
		++lightIndex)
	{
		ImGui::Text("Point light %zu shadow: %zu draws, %llu triangles",
			lightIndex,
			graphics.pointShadowDrawCalls[lightIndex],
			static_cast<unsigned long long>(graphics.pointShadowTriangles[lightIndex]));
	}
	ImGui::Separator();
	ImGui::Text("Build time: %.4f ms", render.LastFrameBuildMs());
	ImGui::Text("Main visibility tests: %.4f ms", graphics.mainVisibilityTestMs);
	ImGui::Text("Shadow pass: %.4f ms", graphics.shadowPassMs);
	ImGui::Text("Flush time: %.4f ms", render.LastFrameFlushMs());
	ImGui::Text("Debug overlay: %.4f ms", render.LastFrameDebugOverlayMs());
	ImGui::Text("Editor GUI/MyGUI: %.4f ms", render.LastFrameEditorGuiMs());
	ImGui::Text("Runtime GUI: %.4f ms", render.LastFrameRuntimeGuiMs());
	ImGui::Separator();
	ImGui::Text("Frame allocator capacity: %.2f KB", static_cast<double>(render.FrameAllocatorCapacityBytes()) / 1024.0);
	ImGui::Text("Frame allocator used: %.2f KB", static_cast<double>(render.FrameAllocatorUsedBytes()) / 1024.0);
	ImGui::Text("Frame allocator peak: %.2f KB", static_cast<double>(render.FrameAllocatorPeakBytes()) / 1024.0);
	ImGui::End();
}

void Debug::drawGameModeInput(const Input& input)
{
	// Game mode uses this panel to show live input and gameplay state without the editor UI.
	m_lastFps = static_cast<float>(Root::Current().Profiler().SmoothedFps());
	if (m_showStatsWindow)
	{
		DrawRenderStatsWindow();
	}

	if (m_showGameInputWindow)
	{
		bool open = m_showGameInputWindow;
		ImGui::Begin("Game Input", &open);
	ImGui::Text("FPS: %.1f", m_lastFps);
	ImGui::Text("Frame: %.3f ms", Root::Current().Profiler().FrameMs());
	const Input::InputFrame& inputFrame = input.Frame();
	ImGui::Text("Window focused: %s", inputFrame.windowFocused ? "yes" : "no");
	ImGui::Text("Look active: %s", inputFrame.lookActive ? "yes" : "no");
	ImGui::Text("Look became active: %s", inputFrame.lookBecameActive ? "yes" : "no");
	ImGui::Text("Engine mode: %s", Root::Current().State().IsGameMode() ? "Game" : "Editor");
	ImGui::Text("Gameplay flow: %s",
		Root::Current().Gameplay().State() == GameplayManager::GameState::MainMenu ? "MainMenu" :
		Root::Current().Gameplay().State() == GameplayManager::GameState::Playing ? "Playing" :
		Root::Current().Gameplay().State() == GameplayManager::GameState::Paused ? "Paused" : "Unknown");
	ImGui::Text("Runtime UI mode: %s",
		Root::Current().FrontEnd().RuntimeGUI().Mode() == GameGUIManager::UIMode::MainMenu ? "MainMenu" :
		Root::Current().FrontEnd().RuntimeGUI().Mode() == GameGUIManager::UIMode::GameplayHUD ? "GameplayHUD" :
		Root::Current().FrontEnd().RuntimeGUI().Mode() == GameGUIManager::UIMode::PauseMenu ? "PauseMenu" :
		Root::Current().FrontEnd().RuntimeGUI().Mode() == GameGUIManager::UIMode::PlayerUI ? "PlayerUI" :
		Root::Current().FrontEnd().RuntimeGUI().Mode() == GameGUIManager::UIMode::Custom ? "Custom" : "Unknown");
	const Scene* activeLevel = Root::Current().Scenes().ActiveLevel();
	ImGui::Text("Active Scene: %s", activeLevel ? activeLevel->Name().c_str() : "<none>");
	ImGui::Text("Active Scene objects: %zu", activeLevel ? activeLevel->Objects().size() : 0);
	ImGui::Text("Controller components: %zu", Root::Current().Gameplay().ControllerCount());
	const glm::vec3 move = inputFrame.moveInput;
	const glm::vec2 mouse = inputFrame.mouseDelta;
	ImGui::Separator();
	ImGui::Text("Move input: %.2f, %.2f", move.x, move.z);
	ImGui::Text("Mouse delta: %.2f, %.2f", mouse.x, mouse.y);
			ImGui::Text("Delta time: %.4f", inputFrame.deltaTime);
		ImGui::Checkbox("Camera Collision Debug", &m_showCameraCollisionDebug);
	ImGui::Separator();
	ImGui::Separator();
	ImGui::TextUnformatted("Recent logs:");
	const std::size_t logCount = m_logMessages.size();
	const std::size_t startIndex = logCount > 5 ? logCount - 5 : 0;
	for (std::size_t i = startIndex; i < logCount; ++i)
	{
		ImGui::BulletText("%s", m_logMessages[i].c_str());
	}
		ImGui::End();
		m_showGameInputWindow = open;
	}

	if (m_showPhysicsDiagnosticsWindow)
	{
		bool open = m_showPhysicsDiagnosticsWindow;
		ImGui::Begin("Physics Diagnostics", &open);
		ImGui::Text("Camera: %.3f, %.3f, %.3f", m_physicsCameraPosition.x, m_physicsCameraPosition.y, m_physicsCameraPosition.z);
		ImGui::Text("Desired: %.3f, %.3f, %.3f", m_physicsDesiredPosition.x, m_physicsDesiredPosition.y, m_physicsDesiredPosition.z);
		ImGui::Text("Resolved: %.3f, %.3f, %.3f", m_physicsResolvedPosition.x, m_physicsResolvedPosition.y, m_physicsResolvedPosition.z);
		ImGui::Text("Collider radius: %.3f", m_physicsColliderRadius);
		ImGui::Text("Collision count: %d", m_physicsCollisionCount);
		ImGui::Text("Last object: %s", m_physicsCollisionObject.empty() ? "<none>" : m_physicsCollisionObject.c_str());
		ImGui::Text("Normal: %.3f, %.3f, %.3f", m_physicsCollisionNormal.x, m_physicsCollisionNormal.y, m_physicsCollisionNormal.z);
		ImGui::Text("Penetration: %.3f", m_physicsPenetration);
		ImGui::Separator();
		ImGui::Text("IsGrounded true transitions: %d", m_groundedTrueCount);
		ImGui::Text("IsGrounded false transitions: %d", m_groundedFalseCount);
		ImGui::TextWrapped("Last grounded transition: %s", m_lastGroundedTransition.empty() ? "<none>" : m_lastGroundedTransition.c_str());
		ImGui::Separator();
		ImGui::Text("Controller: %s", m_controllerPhysicsObject.empty() ? "<none>" : m_controllerPhysicsObject.c_str());
		ImGui::Text("Controller collider: %s", m_controllerColliderValid ? "valid" : "missing");
		ImGui::Text("Controller bounds: %s", m_controllerBoundsValid ? "valid" : "invalid");
		ImGui::Text("Vertical sweep hit: %s", m_controllerVerticalSweepHit ? "true" : "false");
		ImGui::Text("Vertical sweep time: %.4f", m_controllerVerticalSweepTime);
		ImGui::Text("Sweep normal: %.3f, %.3f, %.3f", m_controllerSweepNormal.x, m_controllerSweepNormal.y, m_controllerSweepNormal.z);
		ImGui::Text("Controller velocity: %.3f, %.3f, %.3f", m_controllerVelocity.x, m_controllerVelocity.y, m_controllerVelocity.z);
		ImGui::Text("Controller grounded: %s", m_controllerGrounded ? "true" : "false");
		ImGui::End();
		m_showPhysicsDiagnosticsWindow = open;
	}

	if (Root::Current().Profiler().IsEnabled())
	{
		bool open = true;
		ImGui::Begin("Profiler", &open, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
		for (const FrameProfiler::Sample& sample : Root::Current().Profiler().Samples())
		{
			ImGui::Text(
				"%-16s current %7.3f ms | avg %7.3f ms | max %7.3f ms",
				sample.name.c_str(),
				sample.currentMs,
				sample.averageMs,
				sample.maximumMs);
		}
		ImGui::End();
		if (!open)
		{
			Root::Current().Profiler().SetEnabled(false);
		}
	}

	if (m_showGameplayDiagnosticsWindow)
	{
		bool open = m_showGameplayDiagnosticsWindow;
		ImGui::Begin("Gameplay Diagnostics", &open);
	ImGui::Text("Object: %s", m_gameplayObjectName.empty() ? "<none>" : m_gameplayObjectName.c_str());
	ImGui::Text("Active Scene: %s", m_activeLevelName.empty() ? "<none>" : m_activeLevelName.c_str());
	ImGui::Text("Active Scene objects: %zu", m_activeLevelObjects);
		ImGui::Separator();
		ImGui::Separator();
	ImGui::Text("Move input: %.2f, %.2f, %.2f", m_gameplayMoveInput.x, m_gameplayMoveInput.y, m_gameplayMoveInput.z);
	ImGui::Text("Move speed: %.2f", m_gameplayMoveSpeed);
	ImGui::Text("Delta time: %.4f", m_gameplayDt);
	ImGui::Text("Applied delta: %.3f, %.3f, %.3f", m_gameplayDelta.x, m_gameplayDelta.y, m_gameplayDelta.z);
		ImGui::Text("Ground surface angle: %.2f degrees", m_gameplayGroundSurfaceAngle);
		ImGui::Text("Is grounded: %s", m_gameplayGrounded ? "true" : "false");
		ImGui::Text("Position: %.3f, %.3f, %.3f", m_gameplayPosition.x, m_gameplayPosition.y, m_gameplayPosition.z);
		if (m_showMotionDiagnostics)
		{
			ImGui::Begin("Motion Diagnostics");
			const PathedCamera& camera = Root::Current().Render().GetPathedCamera();
			ImGui::Separator();
		ImGui::Text("Render dt: %.5f s (%.3f ms)", m_gameplayDt, m_gameplayDt * 1000.0f);
		ImGui::Text("Input move: %.3f, %.3f, %.3f", m_gameplayMoveInput.x, m_gameplayMoveInput.y, m_gameplayMoveInput.z);
			ImGui::Text("Applied movement: %.5f, %.5f, %.5f", m_gameplayDelta.x, m_gameplayDelta.y, m_gameplayDelta.z);
			ImGui::Text("Player position: %.5f, %.5f, %.5f", m_gameplayPosition.x, m_gameplayPosition.y, m_gameplayPosition.z);
			ImGui::Text("Camera position: %.5f, %.5f, %.5f", camera.GetPosition().x, camera.GetPosition().y, camera.GetPosition().z);
			ImGui::End();
		}
		if (m_showCameraDiagnostics)
		{
			ImGui::Begin("Camera Diagnostics");
			const CameraDiagnosticsSnapshot& camera = m_cameraDiagnostics;
			ImGui::Separator();
			ImGui::TextUnformatted("Control");
			ImGui::Text("Look: %.3f, %.3f | yaw: %.2f | dt: %.4f",
				camera.lookInput.x, camera.lookInput.y, camera.yaw, camera.dt);
			ImGui::Text("Camera before: %.3f, %.3f, %.3f",
				camera.cameraPositionBefore.x, camera.cameraPositionBefore.y,
				camera.cameraPositionBefore.z);

			ImGui::Separator();
			ImGui::TextUnformatted("Solver");
			ImGui::Text("Valid: %s | side: %d -> %d | offset: %.2f",
				camera.solverValid ? "yes" : "no",
				camera.previousAvoidanceSide, camera.selectedAvoidanceSide,
				camera.selectedYawOffset);
			ImGui::Text("Resolution active: %s -> %s",
				camera.resolutionActiveBefore ? "yes" : "no",
				camera.resolutionActiveAfter ? "yes" : "no");
			ImGui::Text("Solver position: %.3f, %.3f, %.3f",
				camera.solverPosition.x, camera.solverPosition.y, camera.solverPosition.z);
			ImGui::Text("Solver overlap: %s | LOS: %s",
				camera.solverOverlaps ? "yes" : "no",
				camera.solverHasLineOfSight ? "yes" : "no");

			ImGui::Separator();
			ImGui::TextUnformatted("Collision");
			ImGui::Text("Smoothing: %s | escape: %s | rejected: %s",
				camera.smoothingAttempted ? "yes" : "no",
				camera.usedImmediateEscape ? "yes" : "no",
				camera.smoothedPositionRejected ? "yes" : "no");
			ImGui::Text("Applied movement: %.3f, %.3f, %.3f",
				camera.appliedMovement.x, camera.appliedMovement.y,
				camera.appliedMovement.z);
			ImGui::Text("Sweep hits: %d | object: %s | shape: %s",
				camera.sweepHitCount,
				camera.lastSweepObject.empty() ? "<none>" : camera.lastSweepObject.c_str(),
				camera.lastSweepShape.empty() ? "<none>" : camera.lastSweepShape.c_str());

			ImGui::Separator();
			ImGui::TextUnformatted("Committed");
			ImGui::Text("Position: %.3f, %.3f, %.3f",
				camera.committedPosition.x, camera.committedPosition.y,
				camera.committedPosition.z);
			ImGui::Text("Overlap: %s | LOS: %s | last valid: %s",
				camera.committedOverlaps ? "yes" : "no",
				camera.committedHasLineOfSight ? "yes" : "no",
				camera.hasLastValidPosition ? "yes" : "no");

			ImGui::Separator();
			ImGui::TextUnformatted("Recent Camera Transitions");
			ImGui::BeginChild("CameraDiagnosticsEvents", ImVec2(0.0f, 150.0f), true);
			if (m_cameraDiagnosticsEvents.empty())
			{
				ImGui::TextDisabled("No resolver state changes captured yet.");
			}
			else
			{
				for (auto event = m_cameraDiagnosticsEvents.rbegin();
					event != m_cameraDiagnosticsEvents.rend(); ++event)
				{
					ImGui::TextWrapped("%s", event->c_str());
				}
			}
			ImGui::EndChild();
			ImGui::End();
		}
		ImGui::End();
		m_showGameplayDiagnosticsWindow = open;
	}

	if (m_showPathedCameraDiagnostics)
	{
		bool open = m_showPathedCameraDiagnostics;
		ImGui::Begin("Pathed Camera Diagnostics", &open);
		const PathedCamera& camera = Root::Current().Render().GetPathedCamera();
		const CameraPathData& path = camera.Path();
		ImGui::Text("Path points: %zu", path.points.size());
		ImGui::Text("Target: %s", camera.Target() ? "assigned" : "none");
		ImGui::Text("Player progress: %.3f", camera.PlayerProgress());
		ImGui::Text("Follow sharpness: %.3f", camera.FollowSharpness());
		ImGui::Text("Curve samples/segment: %d", camera.PathSamplesPerSegment());
		ImGui::Text("Position: %.3f, %.3f, %.3f", camera.GetPosition().x, camera.GetPosition().y, camera.GetPosition().z);
		ImGui::Text("Facing: %.3f, %.3f, %.3f", camera.GetFacing().x, camera.GetFacing().y, camera.GetFacing().z);
		ImGui::End();
		m_showPathedCameraDiagnostics = open;
	}

	if (m_showEntityStateDiagnosticsWindow)
	{
		bool open = m_showEntityStateDiagnosticsWindow;
		ImGui::Begin("Entity State Diagnostics", &open);
		Scene* activeScene = Root::Current().Scenes().ActiveLevel();
		const std::vector<std::unique_ptr<Entity>>* objects = activeScene ? &activeScene->Objects() : nullptr;
		if (!objects || objects->empty())
		{
			ImGui::TextDisabled("No active scene entities.");
		}
		else
		{
			std::vector<Entity*> selectableEntities;
			selectableEntities.reserve(objects->size());
			for (const std::unique_ptr<Entity>& object : *objects)
			{
				if (object)
				{
					selectableEntities.push_back(object.get());
				}
			}

			if (m_entityStateDiagnosticsSelectedEntityId == 0 ||
				std::none_of(selectableEntities.begin(), selectableEntities.end(), [this](Entity* entity)
				{
					return entity->Id() == m_entityStateDiagnosticsSelectedEntityId;
				}))
			{
				m_entityStateDiagnosticsSelectedEntityId = selectableEntities.front()->Id();
			}

			Entity* selectedEntity = nullptr;
			for (Entity* entity : selectableEntities)
			{
				if (entity->Id() == m_entityStateDiagnosticsSelectedEntityId)
				{
					selectedEntity = entity;
					break;
				}
			}

			if (ImGui::BeginCombo("Entity", selectedEntity ? selectedEntity->Name().c_str() : "<select entity>"))
			{
				for (Entity* entity : selectableEntities)
				{
					const bool selected = entity->Id() == m_entityStateDiagnosticsSelectedEntityId;
					if (ImGui::Selectable(entity->Name().c_str(), selected))
					{
						m_entityStateDiagnosticsSelectedEntityId = entity->Id();
						selectedEntity = entity;
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (selectedEntity)
			{
				ImGui::Separator();
				ImGui::Text("Entity: %s", selectedEntity->Name().c_str());
				ImGui::Text("Id: %u", selectedEntity->Id());
				if (EntityStateMachine* stateMachine = selectedEntity->GetEntityState())
				{
					ImGui::Separator();
					ImGui::TextUnformatted("State Machine");
					ImGui::Text("Current state: %s", stateMachine->CurrentState().empty() ? "<none>" : stateMachine->CurrentState().c_str());
					ImGui::Text("Desired state: %s", stateMachine->DesiredState().empty() ? "<none>" : stateMachine->DesiredState().c_str());
					ImGui::Text("Initial state: %s", stateMachine->InitialState().empty() ? "<none>" : stateMachine->InitialState().c_str());
					ImGui::Text("Locked until complete: %s", stateMachine->CurrentStateLockedUntilComplete() ? "yes" : "no");
					ImGui::Text("Wait for current state to finish: %s", stateMachine->CurrentStateWaitsForCompletion() ? "yes" : "no");
					ImGui::Text("Blocks movement: %s", stateMachine->CurrentStateBlocksMovement() ? "yes" : "no");
					ImGui::Text("Blocks input: %s", stateMachine->CurrentStateBlocksInput() ? "yes" : "no");
					const float clipDuration = stateMachine->CurrentStateClipDurationSeconds();
					const float elapsed = stateMachine->CurrentStateElapsedSeconds();
					const float remaining = stateMachine->CurrentStateSecondsUntilUnlock();
					const std::string remainingText = stateMachine->CurrentStateLockedUntilComplete()
						? (remaining > 0.0f ? FormatBindableValueForDebug(remaining) + " s" : "0.000 s")
						: "not locked";
					ImGui::Text("Animation time: %.3f / %.3f s", elapsed, clipDuration);
					ImGui::Text("Time until unlock: %s", remainingText.c_str());
					ImGui::Separator();
					ImGui::TextUnformatted("Transition Gating");
					ImGui::Text("Last transition evaluated: %s -> %s",
						stateMachine->LastTransitionFrom().empty() ? "<not evaluated yet>" : stateMachine->LastTransitionFrom().c_str(),
						stateMachine->LastTransitionTo().empty() ? "<not evaluated yet>" : stateMachine->LastTransitionTo().c_str());
					ImGui::Text("Condition passed: %s", stateMachine->LastTransitionPassed() ? "yes" : "no");
					ImGui::Text("Wait gate blocked: %s", stateMachine->LastTransitionWaitBlocked() ? "yes" : "no");
					ImGui::Text("Block reason: %s",
						stateMachine->LastTransitionBlockedReason().empty() ? "<none>" : stateMachine->LastTransitionBlockedReason().c_str());
					ImGui::Separator();
					ImGui::TextUnformatted("Outgoing Transitions");
					const std::vector<EntityStateMachine::Transition>& transitions = stateMachine->Transitions();
					const std::string currentState = stateMachine->CurrentState();
					const auto conditionToText = [](const EntityStateMachine::Condition& condition)
					{
						std::string text = EntityStateMachine::OperandToString(condition.left);
						text += " ";
						text += EntityStateMachine::ComparatorToString(condition.comparator);
						text += " ";
						text += EntityStateMachine::OperandToString(condition.right);
						return text;
					};
					bool anyOutgoing = false;
					for (const EntityStateMachine::Transition& transition : transitions)
					{
						if (!currentState.empty() && transition.from != currentState && transition.from != "*")
						{
							continue;
						}
						anyOutgoing = true;
						const std::vector<EntityStateMachine::Condition>& conditions = transition.conditions.empty()
							? std::vector<EntityStateMachine::Condition>{ transition.condition }
							: transition.conditions;
						std::string conditionText;
						for (std::size_t i = 0; i < conditions.size(); ++i)
						{
							if (i > 0)
							{
								conditionText += " AND ";
							}
							conditionText += conditionToText(conditions[i]);
						}
						const bool isLastTransition =
							stateMachine->LastTransitionFrom() == transition.from &&
							stateMachine->LastTransitionTo() == transition.to;
						if (isLastTransition)
						{
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.82f, 0.20f, 1.0f));
						}
						ImGui::BulletText(
							"%s -> %s | wait: %s | blend: %.2f s | when: %s",
							transition.from.c_str(),
							transition.to.c_str(),
							transition.waitForCurrentStateComplete ? "yes" : "no",
							transition.blendSeconds,
							conditionText.empty() ? "<none>" : conditionText.c_str());
						if (isLastTransition)
						{
							ImGui::PopStyleColor();
						}
					}
					if (!anyOutgoing)
					{
						ImGui::TextDisabled("No outgoing transitions from the current state.");
					}
				}
				else
				{
					ImGui::Separator();
					ImGui::TextDisabled("No EntityStateMachine on this entity.");
				}

				ImGui::Separator();
				ImGui::TextUnformatted("Bindable Values");
				const std::vector<DebugBindableGroup> groups = CollectBindableGroups(*selectedEntity);
				if (groups.empty())
				{
					ImGui::TextDisabled("No bindable values.");
				}
				else
				{
					for (const DebugBindableGroup& group : groups)
					{
						DrawBindableGroup(group, *selectedEntity);
					}
				}
			}
		}
		ImGui::End();
		m_showEntityStateDiagnosticsWindow = open;
	}

	if (m_showAnimationDiagnosticsWindow)
	{
		bool open = m_showAnimationDiagnosticsWindow;
		ImGui::Begin("Animation Diagnostics", &open);
	ImGui::Text("Current state: %s", m_animationCurrentState.empty() ? "<none>" : m_animationCurrentState.c_str());
	ImGui::Text("Desired state: %s", m_animationDesiredState.empty() ? "<none>" : m_animationDesiredState.c_str());
	ImGui::TextWrapped("Last transition: %s", m_animationLastTransitionDebug.empty() ? "<none>" : m_animationLastTransitionDebug.c_str());
	ImGui::Text("Last transition from: %s", m_animationLastTransitionFrom.empty() ? "<none>" : m_animationLastTransitionFrom.c_str());
	ImGui::Text("Last transition to: %s", m_animationLastTransitionTo.empty() ? "<none>" : m_animationLastTransitionTo.c_str());
	ImGui::Text("Last condition: %s %s %s",
		m_animationLastTransitionLeftOperandText.empty() ? "<none>" : m_animationLastTransitionLeftOperandText.c_str(),
		m_animationLastTransitionComparatorText.empty() ? "<none>" : m_animationLastTransitionComparatorText.c_str(),
		m_animationLastTransitionRightOperandText.empty() ? "<none>" : m_animationLastTransitionRightOperandText.c_str());
	ImGui::Text("Resolved operand values: %.3f %s %.3f",
		m_animationLastTransitionLeftValue,
		m_animationLastTransitionComparatorText.empty() ? "<none>" : m_animationLastTransitionComparatorText.c_str(),
		m_animationLastTransitionRightValue);
	ImGui::Text("Condition passed: %s", m_animationLastTransitionPassed ? "yes" : "no");
	ImGui::Text("Resolved target: %s", m_animationLastResolvedTargetState.empty() ? "<none>" : m_animationLastResolvedTargetState.c_str());
	ImGui::Text("Resolved clip index: %d", m_animationLastResolvedTargetClipIndex);
	ImGui::Text("Resolved target found: %s", m_animationLastResolvedTargetFound ? "yes" : "no");
	ImGui::Separator();
	ImGui::TextUnformatted("States:");
	ImGui::BeginChild("AnimatorStateList", ImVec2(0.0f, 120.0f), true);
	ImGui::TextUnformatted(m_animationStateListText.empty() ? "<none>" : m_animationStateListText.c_str());
	ImGui::EndChild();
		ImGui::End();
		m_showAnimationDiagnosticsWindow = open;
	}
}

void Debug::DrawPhysicsBoundingVolumes(const Camera& camera)
{
	if (!m_showLevelColliderDebugShapes)
	{
		return;
	}

	const Scene* activeLevel = Root::Current().Scenes().ActiveLevel();
	std::vector<Entity*> currentBoundingBoxObjects;
	if (activeLevel)
	{
		currentBoundingBoxObjects.reserve(activeLevel->Objects().size());
		for (const auto& object : activeLevel->Objects())
		{
			if (object)
			{
				currentBoundingBoxObjects.push_back(object.get());
			}
		}
	}

	if (currentBoundingBoxObjects != m_entityBoundingBoxObjects)
	{
		ClearEntityBoundingBoxes();
		m_entityBoundingBoxObjects = currentBoundingBoxObjects;
		for (Entity* object : m_entityBoundingBoxObjects)
		{
			m_entityBoundingBoxes.push_back(new Line(glm::vec3(0.0f), glm::vec3(0.0f)));
		}
	}

	const glm::mat4 projection = camera.GetProjectionMatrix();
	const glm::mat4 view = camera.GetViewMatrix();
	for (std::size_t i = 0; i < m_entityBoundingBoxObjects.size(); ++i)
	{
		Entity* object = m_entityBoundingBoxObjects[i];
		if (!object || !object->GetMesh() || object->GetController() == nullptr)
		{
			continue;
		}

		glm::vec3 boxMin;
		glm::vec3 boxMax;
		if (!object->WorldAABB(boxMin, boxMax))
		{
			continue;
		}

		Line* volume = m_entityBoundingBoxes[i];
		const glm::vec3 debugColor(1.0f, 0.7f, 0.1f);
		if (object->GetPhysicsColliderShape() == PhysicsColliderShape::Capsule)
		{
			glm::vec3 base;
			glm::vec3 tip;
			float radius = 0.0f;
			if (!BuildPhysicsCapsule(*object, boxMin, boxMax, base, tip, radius))
			{
				BuildVerticalCapsule(boxMin, boxMax, base, tip, radius);
			}
			volume->SetVertices(MakeWireCapsuleVertices(base, tip, radius, debugColor));
		}
		else if (object->GetPhysicsColliderShape() == PhysicsColliderShape::Convex)
		{
			volume->SetVertices(MakeWireConvexVertices(*object, debugColor));
		}
		else
		{
			volume->SetBounds(boxMin, boxMax, debugColor);
		}

		volume->UpdateProjection(projection);
		volume->draw(view);
	}

	std::vector<LevelCollider*> currentLevelColliders;
	if (activeLevel)
	{
		for (const auto& collider : activeLevel->LevelColliders())
			if (collider && collider->DebugVisible()) currentLevelColliders.push_back(collider.get());
	}
	if (currentLevelColliders != m_levelColliderObjects)
	{
		for (Line* volume : m_levelColliderBounds) delete volume;
		for (Line* gizmo : m_levelColliderFaceGizmos) delete gizmo;
		for (Line* highlight : m_levelColliderFaceHighlights) delete highlight;
		m_levelColliderBounds.clear();
		m_levelColliderFaceGizmos.clear();
		m_levelColliderFaceHighlights.clear();
		m_levelColliderObjects = currentLevelColliders;
		for (LevelCollider* collider : m_levelColliderObjects)
			m_levelColliderBounds.push_back(new Line(glm::vec3(0.0f), glm::vec3(0.0f)));
	}
	for (std::size_t i = 0; i < m_levelColliderObjects.size(); ++i)
	{
		LevelCollider* collider = m_levelColliderObjects[i];
		Line* volume = m_levelColliderBounds[i];
		const glm::vec3 debugColor(0.2f, 0.9f, 1.0f);
		if (collider->Shape() == LevelColliderShape::Capsule)
		{
			const float radius = collider->Radius();
			const float halfHeight = std::max(radius, collider->Height() * 0.5f);
			volume->SetVertices(MakeWireCapsuleVertices(
				glm::vec3(0.0f, -(halfHeight - radius), 0.0f),
				glm::vec3(0.0f, halfHeight - radius, 0.0f), radius, debugColor));
		}
		else
		{
			volume->SetBounds(glm::vec3(-50.0f), glm::vec3(50.0f), debugColor);
		}
		glm::mat4 model = glm::translate(glm::mat4(1.0f), collider->Position());
		model = glm::rotate(model, collider->Rotation().z, glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::rotate(model, collider->Rotation().y, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, collider->Rotation().x, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::scale(model, collider->Scale());
		volume->UpdateProjection(projection);
		volume->draw(view, model);
	}

}

bool Debug::ShowCameraCollisionDebug() const { return m_showCameraCollisionDebug; }
void Debug::SetShowCameraCollisionDebug(bool show) { m_showCameraCollisionDebug = show; }
bool Debug::ShowPhysicsDiagnosticsWindow() const { return m_showPhysicsDiagnosticsWindow; }
void Debug::SetShowPhysicsDiagnosticsWindow(bool show) { m_showPhysicsDiagnosticsWindow = show; }
bool Debug::ShowLevelColliderDebugShapes() const { return m_showLevelColliderDebugShapes; }
void Debug::SetShowLevelColliderDebugShapes(bool show) { m_showLevelColliderDebugShapes = show; }

void Debug::SetPhysicsDiagnostics(const glm::vec3& cameraPosition, const glm::vec3& desiredPosition, const glm::vec3& resolvedPosition, float colliderRadius, int collisionCount, const glm::vec3& collisionNormal, float penetration, const std::string& collisionObject)
{
	m_physicsCameraPosition = cameraPosition;
	m_physicsDesiredPosition = desiredPosition;
	m_physicsResolvedPosition = resolvedPosition;
	m_physicsColliderRadius = colliderRadius;
	m_physicsCollisionCount = collisionCount;
	m_physicsCollisionNormal = collisionNormal;
	m_physicsPenetration = penetration;
	m_physicsCollisionObject = collisionObject;
	if (collisionCount > 0 && (++m_physicsDiagnosticsFrame % 30u) == 0u)
	{
		LogTagged("Physics", "Camera collision count=" + std::to_string(collisionCount) +
			" object=" + (collisionObject.empty() ? std::string("<unknown>") : collisionObject) +
			" normal=(" + std::to_string(collisionNormal.x) + "," + std::to_string(collisionNormal.y) + "," + std::to_string(collisionNormal.z) + ")" +
			" penetration=" + std::to_string(penetration) +
			" cameraY=" + std::to_string(cameraPosition.y) +
			" desiredY=" + std::to_string(desiredPosition.y) +
			" resolvedY=" + std::to_string(resolvedPosition.y));
	}
}

void Debug::RecordGroundedTransition(const std::string& objectName, bool grounded, bool rawGrounded,
	bool mainGroundContact, bool probeGroundContact, const glm::vec3& collisionNormal,
	const glm::vec3& position, const glm::vec3& velocity, float groundedLossTimer, float dt)
{
	if (grounded)
	{
		++m_groundedTrueCount;
	}
	else
	{
		++m_groundedFalseCount;
	}

	const std::string state = grounded ? "true" : "false";
	m_lastGroundedTransition = "object: " + objectName +
		"\nstate: " + state +
		"\nraw grounded: " + (rawGrounded ? "true" : "false") +
		"\nmain ground contact: " + (mainGroundContact ? "true" : "false") +
		"\nprobe ground contact: " + (probeGroundContact ? "true" : "false") +
		"\ncollision normal: (" + std::to_string(collisionNormal.x) + ", " +
		std::to_string(collisionNormal.y) + ", " + std::to_string(collisionNormal.z) + ")" +
		"\nposition: (" + std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z) + ")" +
		"\nvelocity: (" + std::to_string(velocity.x) + ", " + std::to_string(velocity.y) + ", " + std::to_string(velocity.z) + ")" +
		"\ngrounded loss timer: " + std::to_string(groundedLossTimer) +
		"\ndt: " + std::to_string(dt);
	LogTagged("Physics", "IsGrounded switched " + state + ": " + m_lastGroundedTransition);
}

void Debug::SetControllerPhysicsDiagnostics(const std::string& objectName, bool colliderValid,
	bool boundsValid, bool verticalSweepHit, float verticalSweepTime,
	const glm::vec3& sweepNormal, const glm::vec3& velocity, bool grounded)
{
	++m_controllerDiagnosticsFrame;
	if ((m_controllerDiagnosticsFrame % 30u) == 0u ||
		objectName != m_controllerPhysicsObject ||
		colliderValid != m_controllerColliderValid ||
		boundsValid != m_controllerBoundsValid ||
		grounded != m_controllerGrounded)
	{
		LogTagged("Controller", objectName +
			" collider=" + (colliderValid ? "valid" : "missing") +
			" bounds=" + (boundsValid ? "valid" : "invalid") +
			" downwardHit=" + (verticalSweepHit ? "true" : "false") +
			" sweepTime=" + std::to_string(verticalSweepTime) +
			" normal=(" + std::to_string(sweepNormal.x) + "," +
			std::to_string(sweepNormal.y) + "," + std::to_string(sweepNormal.z) + ")" +
			" velocity=(" + std::to_string(velocity.x) + "," +
			std::to_string(velocity.y) + "," + std::to_string(velocity.z) + ")" +
			" grounded=" + (grounded ? "true" : "false"));
	}
	m_controllerPhysicsObject = objectName;
	m_controllerColliderValid = colliderValid;
	m_controllerBoundsValid = boundsValid;
	m_controllerVerticalSweepHit = verticalSweepHit;
	m_controllerVerticalSweepTime = verticalSweepTime;
	m_controllerSweepNormal = sweepNormal;
	m_controllerVelocity = velocity;
	m_controllerGrounded = grounded;
}

void Debug::SetGameplayDiagnostics(const std::string& objectName, const glm::vec3& moveInput, float moveSpeed, float dt, const glm::vec3& delta, const glm::vec3& position, float groundSurfaceAngle, bool grounded)
{
	// Gameplay systems push their latest state here so the debug overlay can show it without
	// reaching back into the controller or object layer every frame.
	m_controllerOwnerBound = objectName != "<unbound>";
	m_gameplayObjectName = objectName;
	m_gameplayMoveInput = moveInput;
	m_gameplayMoveSpeed = moveSpeed;
	m_gameplayDt = dt;
	m_gameplayDelta = delta;
	m_gameplayPosition = position;
	m_gameplayGroundSurfaceAngle = groundSurfaceAngle;
	m_gameplayGrounded = grounded;
}

void Debug::SetCameraDiagnostics(const CameraDiagnosticsSnapshot& diagnostics)
{
	if (m_showCameraDiagnostics)
	{
		const bool stateChanged =
			diagnostics.solverValid != m_cameraDiagnostics.solverValid ||
			diagnostics.selectedAvoidanceSide != m_cameraDiagnostics.selectedAvoidanceSide ||
			std::abs(diagnostics.selectedYawOffset -
				m_cameraDiagnostics.selectedYawOffset) > 0.1f ||
			diagnostics.resolutionActiveAfter != m_cameraDiagnostics.resolutionActiveAfter ||
			diagnostics.currentOverlaps != m_cameraDiagnostics.currentOverlaps ||
			diagnostics.currentHasLineOfSight != m_cameraDiagnostics.currentHasLineOfSight ||
			diagnostics.committedOverlaps != m_cameraDiagnostics.committedOverlaps ||
			diagnostics.committedHasLineOfSight != m_cameraDiagnostics.committedHasLineOfSight ||
			diagnostics.usedImmediateEscape != m_cameraDiagnostics.usedImmediateEscape ||
			diagnostics.smoothedPositionRejected !=
				m_cameraDiagnostics.smoothedPositionRejected ||
			diagnostics.sweepHitCount != m_cameraDiagnostics.sweepHitCount ||
			diagnostics.lastSweepObject != m_cameraDiagnostics.lastSweepObject;

		if (stateChanged)
		{
			std::ostringstream event;
			event << '#' << ++m_cameraDiagnosticsEventSequence << std::fixed
				<< std::setprecision(2)
				<< " look=(" << diagnostics.lookInput.x << ',' << diagnostics.lookInput.y << ')'
				<< " yaw=" << diagnostics.yaw
				<< " valid=" << (diagnostics.solverValid ? "Y" : "N")
				<< " side=" << diagnostics.previousAvoidanceSide << "->"
				<< diagnostics.selectedAvoidanceSide
				<< " offset=" << diagnostics.selectedYawOffset
				<< " active=" << (diagnostics.resolutionActiveBefore ? "Y" : "N")
				<< "->" << (diagnostics.resolutionActiveAfter ? "Y" : "N")
				<< " current[overlap=" << (diagnostics.currentOverlaps ? "Y" : "N")
				<< ",los=" << (diagnostics.currentHasLineOfSight ? "Y" : "N") << ']'
				<< " committed[overlap=" << (diagnostics.committedOverlaps ? "Y" : "N")
				<< ",los=" << (diagnostics.committedHasLineOfSight ? "Y" : "N") << ']'
				<< " sweepHits=" << diagnostics.sweepHitCount
				<< " immediate=" << (diagnostics.usedImmediateEscape ? "Y" : "N")
				<< " rejected=" << (diagnostics.smoothedPositionRejected ? "Y" : "N");
			if (!diagnostics.lastSweepObject.empty())
			{
				event << " hit=" << diagnostics.lastSweepObject
					<< '(' << diagnostics.lastSweepShape << ')';
			}

			constexpr std::size_t maximumEvents = 16;
			if (m_cameraDiagnosticsEvents.size() == maximumEvents)
			{
				m_cameraDiagnosticsEvents.erase(m_cameraDiagnosticsEvents.begin());
			}
			m_cameraDiagnosticsEvents.push_back(event.str());
		}
	}
	m_cameraDiagnostics = diagnostics;
}

void Debug::SetAnimationDiagnostics(const std::string& currentState, const std::string& desiredState, const std::string& lastTransitionDebug, const std::string& lastTransitionFrom, const std::string& lastTransitionTo, const std::string& lastTransitionLeftOperandText, const std::string& lastTransitionComparatorText, const std::string& lastTransitionRightOperandText, float lastTransitionLeftValue, float lastTransitionRightValue, bool lastTransitionPassed, const std::string& lastResolvedTargetState, int lastResolvedTargetClipIndex, bool lastResolvedTargetFound, const std::string& stateListText)
{
	const bool transitionFired = lastTransitionDebug.rfind("Transition fired:", 0) == 0;
	if (!currentState.empty() && (currentState != m_animationCurrentState || transitionFired))
	{
		LogTagged("EntityState", "state=" + currentState +
			" desired=" + desiredState +
			" transition=" + lastTransitionDebug);
	}
	m_animationCurrentState = currentState;
	m_animationDesiredState = desiredState;
	m_animationLastTransitionDebug = lastTransitionDebug;
	m_animationLastTransitionFrom = lastTransitionFrom;
	m_animationLastTransitionTo = lastTransitionTo;
	m_animationLastTransitionLeftOperandText = lastTransitionLeftOperandText;
	m_animationLastTransitionComparatorText = lastTransitionComparatorText;
	m_animationLastTransitionRightOperandText = lastTransitionRightOperandText;
	m_animationLastTransitionLeftValue = lastTransitionLeftValue;
	m_animationLastTransitionRightValue = lastTransitionRightValue;
	m_animationLastTransitionPassed = lastTransitionPassed;
	m_animationLastResolvedTargetState = lastResolvedTargetState;
	m_animationLastResolvedTargetClipIndex = lastResolvedTargetClipIndex;
	m_animationLastResolvedTargetFound = lastResolvedTargetFound;
	m_animationStateListText = stateListText;
}

void Debug::SetGameplayContext(const std::string& activeLevelName, std::size_t activeLevelObjects, std::size_t controllerCount, const std::string& engineMode)
{
	m_activeLevelName = activeLevelName;
	m_activeLevelObjects = activeLevelObjects;
	m_controllerCount = controllerCount;
	m_engineMode = engineMode;
}

bool Debug::ShowLogWindow() const { return m_showLogWindow; }
bool Debug::ShowStatsWindow() const { return m_showStatsWindow; }
void Debug::SetShowLogWindow(bool showLogWindow) { m_showLogWindow = showLogWindow; }
void Debug::SetShowStatsWindow(bool showStatsWindow) { m_showStatsWindow = showStatsWindow; }
bool Debug::ShowGameInputWindow() const { return m_showGameInputWindow; }
void Debug::SetShowGameInputWindow(bool show) { m_showGameInputWindow = show; }
bool Debug::ShowGameplayDiagnosticsWindow() const { return m_showGameplayDiagnosticsWindow; }
void Debug::SetShowGameplayDiagnosticsWindow(bool show) { m_showGameplayDiagnosticsWindow = show; }
bool Debug::ShowMotionDiagnostics() const { return m_showMotionDiagnostics; }
void Debug::SetShowMotionDiagnostics(bool show) { m_showMotionDiagnostics = show; }
bool Debug::ShowCameraDiagnostics() const { return m_showCameraDiagnostics; }
void Debug::SetShowCameraDiagnostics(bool show)
{
	if (show && !m_showCameraDiagnostics)
	{
		m_cameraDiagnosticsEvents.clear();
		m_cameraDiagnosticsEventSequence = 0;
	}
	m_showCameraDiagnostics = show;
}
bool Debug::ShowPathedCameraDiagnostics() const { return m_showPathedCameraDiagnostics; }
void Debug::SetShowPathedCameraDiagnostics(bool show) { m_showPathedCameraDiagnostics = show; }
bool Debug::ShowTriggerSpheres() const { return m_showTriggerSpheres; }
void Debug::SetShowTriggerSpheres(bool show) { m_showTriggerSpheres = show; }
bool Debug::ShowPointLightDebugSpheres() const { return m_showPointLightDebugSpheres; }
void Debug::SetShowPointLightDebugSpheres(bool show) { m_showPointLightDebugSpheres = show; }
bool Debug::ShowEntityStateDiagnosticsWindow() const { return m_showEntityStateDiagnosticsWindow; }
void Debug::SetShowEntityStateDiagnosticsWindow(bool show) { m_showEntityStateDiagnosticsWindow = show; }
bool Debug::ShowAnimationDiagnosticsWindow() const { return m_showAnimationDiagnosticsWindow; }
void Debug::SetShowAnimationDiagnosticsWindow(bool show) { m_showAnimationDiagnosticsWindow = show; }

void Debug::LogMessage(const std::string& message)
{
	LogMessage(Severity::Info, message);
}

std::string Debug::SeverityPrefix(Severity severity) const
{
	switch (severity)
	{
	case Severity::Warning: return "[WARN] ";
	case Severity::Error: return "[ERROR] ";
	case Severity::Fatal: return "[FATAL] ";
	case Severity::Info:
	default:
		return "[INFO] ";
	}
}

void Debug::LogMessage(Severity severity, const std::string& message)
{
	const std::string formatted = SeverityPrefix(severity) + message;
	// Keep the log buffer bounded so the UI remains responsive even if a subsystem is noisy.
	m_logMessages.push_back(formatted);
	if (m_logMessages.size() > 180)
	{
		m_logMessages.erase(m_logMessages.begin());
	}
	if (m_runtimeLog.is_open())
	{
		m_runtimeLog << formatted << '\n';
		m_runtimeLog.flush();
	}
}

void Debug::LogTagged(const std::string& tag, const std::string& message)
{
	LogTagged(Severity::Info, tag, message);
}

void Debug::LogTagged(Severity severity, const std::string& tag, const std::string& message)
{
	// Tags group related messages together without requiring a more complicated logging backend.
	LogMessage(severity, "[" + tag + "] " + message);
}

void Debug::LogOnce(const std::string& key, const std::string& message)
{
	// One-shot messages are useful for startup diagnostics and repeating states that should only be reported once.
	if (std::find(m_logOnceKeys.begin(), m_logOnceKeys.end(), key) != m_logOnceKeys.end())
	{
		return;
	}

	m_logOnceKeys.push_back(key);
	LogMessage(message);
}

void Debug::ClearLogs()
{
	m_logMessages.clear();
	m_logOnceKeys.clear();
}

void Debug::LogException(const std::string& context, const std::exception& ex)
{
	// Exceptions are normalized into a consistent error format for the log window.
	LogMessage(Severity::Error, context + ": " + ex.what());
}

bool Debug::Ensure(bool condition, const std::string& context, const std::string& message)
{
	// Use this when failure is non-fatal but still important enough to surface immediately.
	if (condition)
	{
		return true;
	}

	LogMessage(Severity::Error, context + ": " + message);
	return false;
}

bool Debug::CheckOpenGLError(const std::string& context)
{
	// glGetError is stateful, so this is a point-in-time check meant to be called around suspicious GL calls.
	GLenum error = glGetError();
	if (error == GL_NO_ERROR)
	{
		return false;
	}

	std::string message = context + ": OpenGL error 0x" + std::to_string(static_cast<unsigned int>(error));
	LogMessage(Severity::Error, message);
	return true;
}

void Debug::LogBuildInfo()
{
	// These messages are only useful at startup, when you need to confirm the binary
	// was compiled with the expected toolchain, source root, and dependency set.
#ifdef AQUANACT_SOURCE_ROOT
	LogTagged("Build", std::string("Source root: ") + AQUANACT_SOURCE_ROOT);
#endif
#ifdef _MSC_VER
	LogTagged("Build", "MSVC version: " + std::to_string(_MSC_VER));
#endif
#ifdef _MSVC_LANG
	LogTagged("Build", "MSVC language mode: " + std::to_string(_MSVC_LANG));
#endif
#ifdef _DEBUG
	LogTagged("Build", "Configuration: Debug");
#else
	LogTagged("Build", "Configuration: Release");
#endif
#ifdef AQUANACT_VCPKG_TARGET_TRIPLET
	LogTagged("Build", std::string("vcpkg triplet: ") + AQUANACT_VCPKG_TARGET_TRIPLET);
#endif
#ifdef AQUANACT_MYGUI_ENGINE_LIBRARY
	LogTagged("Dependency", std::string("MyGUI engine: ") + AQUANACT_MYGUI_ENGINE_LIBRARY);
#endif
#ifdef AQUANACT_MYGUI_OPENGL_LIBRARY
	LogTagged("Dependency", std::string("MyGUI OpenGL: ") + AQUANACT_MYGUI_OPENGL_LIBRARY);
#endif
#ifdef AQUANACT_FREETYPE_LIBRARY
	LogTagged("Dependency", std::string("FreeType: ") + AQUANACT_FREETYPE_LIBRARY);
#endif
}

void Debug::VerifyDependencies()
{
#ifndef _WIN32
	// The current implementation only probes Windows-style DLL dependencies.
	LogTagged(Severity::Warning, "Dependency", "Runtime dependency verification is only implemented for Windows.");
	return;
#else
	// LoadLibraryA here is used as a cheap runtime smoke test:
	// if the DLL cannot be loaded from the process search path, the app logs a clear warning.
	struct DependencyProbe
	{
		const char* displayName;
		const char* dllName;
	};

	const DependencyProbe probes[] =
	{
		{ "GLFW", "glfw3.dll" },
		{ "FreeType", "freetyped.dll" }
	};

	for (const DependencyProbe& probe : probes)
	{
		HMODULE module = LoadLibraryA(probe.dllName);
		if (!module)
		{
			// This is not fatal by itself, but it means a dependency is missing from the runtime environment.
			LogDependencyHint(probe.displayName, std::string("missing runtime DLL '") + probe.dllName + "'");
			continue;
		}

		FreeLibrary(module);
	}
#endif
}

void Debug::LogDependencyHint(const std::string& dependency, const std::string& details)
{
	// Keep dependency warnings visually distinct from normal build/runtime logs.
	LogTagged(Severity::Warning, "Dependency", dependency + ": " + details);
}

void Debug::SetGridSettings(float size)
{
	if (size <= 0.0f)
	{
		size = 1.0f;
	}

	m_gridSize = size;
	m_gridSpacing = (size / 24.0f > 1.0f) ? (size / 24.0f) : 1.0f;
	m_axisLength = m_gridSize;
	RebuildAxis();
	RebuildGrid();
}

float Debug::GridSize() const
{
	return m_gridSize;
}

float Debug::GridSpacing() const
{
	return m_gridSpacing;
}

double Debug::StartupToFirstDrawMs() const
{
	return m_startupToFirstDrawMs;
}





