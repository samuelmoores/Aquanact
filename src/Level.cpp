#include "Level.h"
#include "Engine.h"

Level::Level()
{

}

std::vector<Object3D> Level::Objects()
{
	return objects;
}

void Level::Load()
{
	m_axis = Axis(10.0f);
	objects.push_back(Object3D(Object3D::cubeVertices, Object3D::cubeFaces));
}

void Level::DrawAxis()
{
	m_axis.UpdateProjection(Engine::Camera->GetProjectionMatrix());
	m_axis.draw(Engine::Camera->GetViewMatrix());
}
