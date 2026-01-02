#include "Object3D.h"



Object3D::Object3D(std::vector<Vertex3D> vertices, std::vector<uint32_t> faces)
{
	m_skinned = false;
	m_mesh = new Mesh(vertices, faces);

	m_shader.load("shaders/texture_perspective.vert", "shaders/texturing.frag");
	//m_shader.load("shaders/phong.vert", "shaders/phong.frag");
	m_shader.activate();
	//  | Description      | Values: ambient,diffuse,specular,shin |
	//	| ---------------- | ------------------------------------- |
	//	| Matte surface    | `glm::vec4(0.3f, 0.7f, 0.1f, 8.0f)`   |
	//	| Shiny surface    | `glm::vec4(0.1f, 0.5f, 1.0f, 128.0f)` |
	//	| Full bright test | `glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)`   |
	//	| Only diffuse     | `glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)`   |
	//	| Only specular    | `glm::vec4(0.0f, 0.0f, 1.0f, 64.0f)`  |
	//                         
	//m_shader.setUniform("material", glm::vec4(1.0f, 0.5f, 0.9f, 228.0f));
	//m_shader.setUniform("ambientColor", glm::vec3(0.2f, 0.2f, 0.2f));
	//m_shader.setUniform("directionalColor", glm::vec3(1, 1, 1));
	//m_shader.setUniform("directionalLight", glm::vec3(0, -1, 0));

	m_position = glm::vec3(0);
	m_rotation = glm::vec3(0);
	m_scale = glm::vec3(1);
}

Object3D::Object3D(const char* modelFile, const char* textureFile, bool skinned)
{
	m_mesh = new Mesh(modelFile, textureFile);
	m_skinned = skinned;

	//  | Description      | Values: ambient,diffuse,specular,shin |
	//	| ---------------- | ------------------------------------- |
	//	| Matte surface    | `glm::vec4(0.3f, 0.7f, 0.1f, 8.0f)`   |
	//	| Shiny surface    | `glm::vec4(0.1f, 0.5f, 1.0f, 128.0f)` |
	//	| Full bright test | `glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)`   |
	//	| Only diffuse     | `glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)`   |
	//	| Only specular    | `glm::vec4(0.0f, 0.0f, 1.0f, 64.0f)`  |
	//  | Fabric surface   | `glm::vec4(0.2f, 0.7f, 0.1f, 8.0f)`   |

	m_shader.load("shaders/phong.vert", "shaders/phong.frag");
	m_shader.activate();                      
	m_shader.setUniform("material", glm::vec4(0.2f, 0.7f, 0.1f, 8.0f));
	m_shader.setUniform("ambientColor", glm::vec3(0.2f, 0.2f, 0.2f));
	m_shader.setUniform("directionalColor", glm::vec3(1, 1, 1));
	m_shader.setUniform("directionalLight", glm::vec3(-3, -3, -3));

	m_position = glm::vec3(0);
	m_rotation = glm::vec3(0);
	m_scale = glm::vec3(1);
}

Object3D::Object3D(const char* modelFile, const char* textureFile, const char* normalMap)
{
	m_mesh = new Mesh(modelFile, textureFile, normalMap);

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

Object3D::~Object3D()
{
}

Mesh* Object3D::GetMesh()
{
	return m_mesh;
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

void Object3D::SetScale(glm::vec3 scale)
{
	m_scale = scale;
}

void Object3D::updateMeshAABB()
{
	m_mesh->updateAABB(m_position, m_scale);
}

bool Object3D::intersectsRayMesh(glm::vec3 origin, glm::vec3& direction)
{
	return m_mesh->intersectsRay(origin, direction);
}

bool Object3D::skinned()
{
	return m_skinned;
}
