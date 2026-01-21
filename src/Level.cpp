#include "Level.h"
#include "Engine.h"

Level::Level()
{
    m_drawAxis = false;
}

std::vector<Object3D*> Level::Objects()
{
	return objects;
}

void Level::Load()
{
	m_axis = Axis(1000.0f, 100.0f);
    std::string filepathString = "models/o.fbx";
    //std::string filepathString = "models/Todd.obj";
    char* filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/floor.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/wall.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);
}

void Level::DrawAxis()
{
    if (m_drawAxis)
    {
	    m_axis.UpdateProjection(Engine::Camera->GetProjectionMatrix());
	    m_axis.draw(Engine::Camera->GetViewMatrix());
    }
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

    Object3D* object = new Object3D(filepath);

	objects.push_back(object);
    Engine::Camera->Focus(objects[0]->GetMesh()->minBounds(), objects[0]->GetMesh()->maxBounds());

}

void Level::SetDrawAxis(bool drawAxis)
{
    m_drawAxis = drawAxis;
}
