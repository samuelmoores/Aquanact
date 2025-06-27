#include "Object3D.h"

Object3D::Object3D(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces)
{
	//m_shader.load("shaders/texture_perspective.vert", "shaders/texturing.frag");
	m_shader.load("shaders/phong.vert", "shaders/phong.frag");
	m_shader.activate();
	//  | Description      | Values: ambient,diffuse,specular,shin |
	//	| ---------------- | ------------------------------------- |
	//	| Matte surface    | `glm::vec4(0.3f, 0.7f, 0.1f, 8.0f)`   |
	//	| Shiny surface    | `glm::vec4(0.1f, 0.5f, 1.0f, 128.0f)` |
	//	| Full bright test | `glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)`   |
	//	| Only diffuse     | `glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)`   |
	//	| Only specular    | `glm::vec4(0.0f, 0.0f, 1.0f, 64.0f)`  |
	//                         
	m_shader.setUniform("material", glm::vec4(1.0f, 0.5f, 0.9f, 228.0f));
	m_shader.setUniform("ambientColor", glm::vec3(0.2f, 0.2f, 0.2f));
	m_shader.setUniform("directionalColor", glm::vec3(1, 1, 1));
	m_shader.setUniform("directionalLight", glm::vec3(0, -1, 0));

	Mesh mesh(vertices, faces);
	m_mesh = mesh;

	m_minBounds = glm::vec3(-0.5f, -0.5f, -0.5f);
	m_maxBounds = glm::vec3(0.5f, 0.5f, 0.5f);

	m_meshMinBounds = m_minBounds;
	m_meshMaxBounds = m_maxBounds;

	m_position = glm::vec3(0);
	m_rotation = glm::vec3(0);
	m_scale = glm::vec3(1);
}

Object3D::Object3D(const char* modelFile, const char* textureFile)
{
	Mesh mesh(modelFile, textureFile);
	m_mesh = mesh;

	m_shader.load("shaders/phong.vert", "shaders/phong.frag");
	m_shader.activate();
	//  | Description      | Values: ambient,diffuse,specular,shin |
	//	| ---------------- | ------------------------------------- |
	//	| Matte surface    | `glm::vec4(0.3f, 0.7f, 0.1f, 8.0f)`   |
	//	| Shiny surface    | `glm::vec4(0.1f, 0.5f, 1.0f, 128.0f)` |
	//	| Full bright test | `glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)`   |
	//	| Only diffuse     | `glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)`   |
	//	| Only specular    | `glm::vec4(0.0f, 0.0f, 1.0f, 64.0f)`  |
	//                         
	m_shader.setUniform("material", glm::vec4(1.0f, 0.5f, 0.9f, 228.0f));
	m_shader.setUniform("ambientColor", glm::vec3(0.2f, 0.2f, 0.2f));
	m_shader.setUniform("directionalColor", glm::vec3(1, 1, 1));
	m_shader.setUniform("directionalLight", glm::vec3(-3, -3, -3));

	m_position = glm::vec3(0);
	m_rotation = glm::vec3(0);
	m_scale = glm::vec3(1);
}

Object3D::Object3D(const char* modelFile, const char* textureFile, const char* normalMap)
{
	Mesh mesh(modelFile, textureFile, normalMap);
	m_mesh = mesh;

	m_shader.load("shaders/phong.vert", "shaders/phong.frag");
	m_shader.activate();
	//  | Description      | Values: ambient,diffuse,specular,shin |
	//	| ---------------- | ------------------------------------- |
	//	| Matte surface    | `glm::vec4(0.3f, 0.7f, 0.1f, 8.0f)`   |
	//	| Shiny surface    | `glm::vec4(0.1f, 0.5f, 1.0f, 128.0f)` |
	//	| Full bright test | `glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)`   |
	//	| Only diffuse     | `glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)`   |
	//	| Only specular    | `glm::vec4(0.0f, 0.0f, 1.0f, 64.0f)`  |
	//                         
	m_shader.setUniform("material", glm::vec4(1.0f, 0.5f, 0.9f, 228.0f));
	m_shader.setUniform("ambientColor", glm::vec3(0.2f, 0.2f, 0.2f));
	m_shader.setUniform("directionalColor", glm::vec3(1, 1, 1));
	m_shader.setUniform("directionalLight", glm::vec3(-3, -3, -3));

	m_position = glm::vec3(0);
	m_rotation = glm::vec3(0);
	m_scale = glm::vec3(1);
}

Mesh* Object3D::GetMesh()
{
	return &m_mesh;
}

ShaderProgram* Object3D::GetShader()
{
	return &m_shader;
}

glm::mat4 Object3D::BuildModelMatrix()
{
	auto m = glm::translate(glm::mat4(1), m_position);
	m = glm::rotate(m, m_rotation[1], glm::vec3(0, 1, 0));
	m = glm::rotate(m, m_rotation[0], glm::vec3(1, 0, 0));
	m = glm::rotate(m, m_rotation[2], glm::vec3(0, 0, 1));
	m = glm::scale(m, m_scale);

	return m;
}

void Object3D::Rotate(glm::vec3 delta)
{
	m_rotation += delta;
}

void Object3D::Move(glm::vec3 delta)
{
	m_position += delta;
}

void Object3D::Scale(glm::vec3 delta)
{
	m_scale += delta;
}

void Object3D::updateAABB()
{
	m_minBounds = m_position + m_meshMinBounds;
	m_maxBounds = m_position + m_meshMaxBounds;

	// Ensure correct ordering if scale is negative
	for (int i = 0; i < 3; ++i) {
		if (m_minBounds[i] > m_maxBounds[i])
			std::swap(m_minBounds[i], m_maxBounds[i]);
	}
}

bool Object3D::intersectsRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const
{
	float tMin = 0.0f;
	float tMax = 1e6f;

	for (int i = 0; i < 3; ++i) {
		if (std::abs(rayDir[i]) < 1e-6f) {
			// Ray is parallel to slab
			if (rayOrigin[i] < m_minBounds[i] || rayOrigin[i] > m_maxBounds[i])
				return false;
		}
		else {
			float ood = 1.0f / rayDir[i];
			float t1 = (m_minBounds[i] - rayOrigin[i]) * ood;
			float t2 = (m_maxBounds[i] - rayOrigin[i]) * ood;
			if (t1 > t2) std::swap(t1, t2);

			tMin = std::max(tMin, t1);
			tMax = std::min(tMax, t2);

			if (tMin > tMax)
				return false;
		}
	}

	return true;
}




