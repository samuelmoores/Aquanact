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

    //Player
    std::string filepathString = "models/villager.fbx";
    char* filepath = filepathString.data();
    LoadObject(filepath);
    Mesh* mesh = objects[0]->GetMesh();
    Engine::Camera->Focus(mesh->minBounds(), mesh->maxBounds());

    // =============================================================

    filepathString = "models/sidewall_01.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/sidewall_02.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/sidewall_03.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/logfloor.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/railing.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/lowerroof.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/frontwall.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/door.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/upperroof.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/upperroof_wood.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    filepathString = "models/windows.fbx";
    filepath = filepathString.data();
    LoadObject(filepath);

    // ================================================================

    //ground
    filepathString = "models/ground.fbx";
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

}

void Level::SetDrawAxis(bool drawAxis)
{
    m_drawAxis = drawAxis;
}
