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
	m_axis = Axis(1000.0f, 100.0f);
    std::string filepathString = "models/Cube.fbx";
    char* filepath = filepathString.data();
    LoadObject(filepath);
}

void Level::DrawAxis()
{
	m_axis.UpdateProjection(Engine::Camera->GetProjectionMatrix());
	m_axis.draw(Engine::Camera->GetViewMatrix());
}

void Level::LoadObject(char filepath[])
{
    int i = 0, j = 0;

    while (filepath[i] != '\0') 
    {
        if (filepath[i] != '"') 
        {
            filepath[j] = filepath[i];
            j++;
        }
        i++;
    }

    filepath[j] = '\0';

	//objects.push_back(Object3D(Object3D::cubeVertices, Object3D::cubeFaces));
	objects.push_back(Object3D(filepath));
    Engine::Camera->Focus(objects[0].GetMesh()->minBounds(), objects[0].GetMesh()->maxBounds());
}
